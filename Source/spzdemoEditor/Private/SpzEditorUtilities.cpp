// Copyright Epic Games, Inc. All Rights Reserved.

#include "SpzEditorUtilities.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "MaterialEditingLibrary.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "NiagaraConstants.h"
#include "NiagaraDataInterfaceArrayFloat.h"
#include "NiagaraEditorUtilities.h"
#include "NiagaraEmitter.h"
#include "NiagaraModule.h"
#include "NiagaraNodeAssignment.h"
#include "NiagaraRendererProperties.h"
#include "NiagaraSpriteRendererProperties.h"
#include "NiagaraSystem.h"
#include "NiagaraSystemEditorData.h"
#include "NiagaraSystemFactoryNew.h"
#include "NiagaraValidationRules.h"
#include "SpzNiagaraParameters.h"
#include "SpzNiagaraPointCloudActor.h"
#include "SpzPointCloudAsset.h"
#include "Factories/MaterialFactoryNew.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionConstant2Vector.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionSphereMask.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionVertexColor.h"
#include "UObject/Package.h"
#include "ViewModels/NiagaraEmitterHandleViewModel.h"
#include "ViewModels/NiagaraEmitterViewModel.h"
#include "ViewModels/NiagaraSystemViewModel.h"
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"
#include "ViewModels/Stack/NiagaraStackFunctionInput.h"
#include "ViewModels/Stack/NiagaraStackModuleItem.h"

namespace
{
	constexpr TCHAR NiagaraSystemAssetName[] = TEXT("NS_SPZPointCloud");
	constexpr TCHAR PointSpriteMaterialAssetName[] = TEXT("M_SPZPointSprite");
	constexpr TCHAR NiagaraBlueprintPrefix[] = TEXT("BP_");

	constexpr TCHAR SimpleSpriteBurstEmitterPath[] = TEXT("/Niagara/DefaultAssets/Templates/Emitters/SimpleSpriteBurst.SimpleSpriteBurst");
	constexpr TCHAR SpawnBurstModulePath[] = TEXT("/Niagara/Modules/Emitter/SpawnBurst_Instantaneous.SpawnBurst_Instantaneous");
	constexpr TCHAR InitializeParticleModulePath[] = TEXT("/Niagara/Modules/Spawn/Initialization/V2/InitializeParticle.InitializeParticle");
	constexpr TCHAR ParticleStateModulePath[] = TEXT("/Niagara/Modules/Update/Lifetime/ParticleState.ParticleState");
	constexpr TCHAR SelectPositionFromArrayPath[] = TEXT("/Niagara/DynamicInputs/Arrays/SelectPositionFromArray.SelectPositionFromArray");
	constexpr TCHAR SelectColorFromArrayPath[] = TEXT("/Niagara/DynamicInputs/Arrays/SelectLinearColorFromArray.SelectLinearColorFromArray");
	constexpr TCHAR SelectVector4FromArrayPath[] = TEXT("/Niagara/DynamicInputs/Arrays/SelectVector4_FromArray.SelectVector4_FromArray");
	constexpr TCHAR SelectVector2FromArrayPath[] = TEXT("/Niagara/DynamicInputs/Arrays/SelectVector2DFromArray.SelectVector2DFromArray");
	constexpr TCHAR DefaultSpriteMaterialPath[] = TEXT("/Niagara/DefaultAssets/DefaultSpriteMaterial.DefaultSpriteMaterial");

	FString MakeObjectPath(const FString& PackagePath, const FString& AssetName)
	{
		return FString::Printf(TEXT("%s/%s.%s"), *PackagePath, *AssetName, *AssetName);
	}

	FNiagaraVariable MakeUserParameter(const FNiagaraTypeDefinition& TypeDefinition, FName ParameterName)
	{
		return FNiagaraVariable(TypeDefinition, ParameterName);
	}

	const FNiagaraVariable& GetExecIndexParameter()
	{
		static const FNiagaraVariable ExecIndex = MakeUserParameter(FNiagaraTypeDefinition::GetIntDef(), TEXT("Engine.ExecIndex"));
		return ExecIndex;
	}

	FNiagaraVariable GetPointCountParameter()
	{
		static const FNiagaraVariable Variable = MakeUserParameter(FNiagaraTypeDefinition::GetIntDef(), SpzNiagaraParameters::PointCount);
		return Variable;
	}

	FNiagaraVariable GetLifetimeParameter()
	{
		static const FNiagaraVariable Variable = MakeUserParameter(FNiagaraTypeDefinition::GetFloatDef(), SpzNiagaraParameters::ParticleLifetime);
		return Variable;
	}

	FNiagaraVariable GetPositionsParameter()
	{
		static const FNiagaraVariable Variable = MakeUserParameter(FNiagaraTypeDefinition(UNiagaraDataInterfaceArrayPosition::StaticClass()), SpzNiagaraParameters::Positions);
		return Variable;
	}

	FNiagaraVariable GetColorsParameter()
	{
		static const FNiagaraVariable Variable = MakeUserParameter(FNiagaraTypeDefinition(UNiagaraDataInterfaceArrayColor::StaticClass()), SpzNiagaraParameters::Colors);
		return Variable;
	}

	FNiagaraVariable GetDynamicMaterialParametersParameter()
	{
		static const FNiagaraVariable Variable = MakeUserParameter(FNiagaraTypeDefinition(UNiagaraDataInterfaceArrayFloat4::StaticClass()), SpzNiagaraParameters::DynamicMaterialParameters);
		return Variable;
	}

	FNiagaraVariable GetSpriteSizesParameter()
	{
		static const FNiagaraVariable Variable = MakeUserParameter(FNiagaraTypeDefinition(UNiagaraDataInterfaceArrayFloat2::StaticClass()), SpzNiagaraParameters::SpriteSizes);
		return Variable;
	}

	template <typename TExpression>
	TExpression* CreateMaterialExpression(UMaterial& Material, int32 NodePosX, int32 NodePosY)
	{
		return Cast<TExpression>(UMaterialEditingLibrary::CreateMaterialExpression(&Material, TExpression::StaticClass(), NodePosX, NodePosY));
	}

	bool EnsureUserParameter(UNiagaraSystem& System, const FNiagaraVariable& Parameter, const uint8* DefaultValueData = nullptr)
	{
		bool bModified = false;

		if (System.GetExposedParameters().FindParameterOffset(Parameter, true) == nullptr)
		{
			System.GetExposedParameters().AddParameter(Parameter, true, true);
			bModified = true;
		}

		if (DefaultValueData != nullptr)
		{
			System.GetExposedParameters().SetParameterData(DefaultValueData, Parameter, true);
			bModified = true;
		}

		if (FNiagaraEditorUtilities::UserParameters::GetScriptVariableForUserParameter(Parameter, System) != nullptr)
		{
			bModified = true;
		}

		return bModified;
	}

	TSharedPtr<FNiagaraSystemViewModel> CreateSystemViewModel(UNiagaraSystem& System)
	{
		TSharedPtr<FNiagaraSystemViewModel> SystemViewModel = MakeShared<FNiagaraSystemViewModel>();

		FNiagaraSystemViewModelOptions Options;
		Options.bCanAutoCompile = false;
		Options.bCanModifyEmittersFromTimeline = false;
		Options.bCanSimulate = false;
		Options.bCompileForEdit = false;
		Options.bIsForDataProcessingOnly = true;
		Options.EditMode = ENiagaraSystemViewModelEditMode::SystemAsset;
		Options.MessageLogGuid = System.GetAssetGuid().IsValid() ? System.GetAssetGuid() : FGuid::NewGuid();

		SystemViewModel->Initialize(System, Options);
		return SystemViewModel;
	}

	TArray<UNiagaraStackModuleItem*> GetAllModuleItems(const TSharedPtr<FNiagaraSystemViewModel>& SystemViewModel)
	{
		return NiagaraValidation::GetAllStackEntriesInSystem<UNiagaraStackModuleItem>(SystemViewModel, true);
	}

	UNiagaraStackModuleItem* FindModuleItemByScriptPath(const TArray<UNiagaraStackModuleItem*>& ModuleItems, const FString& ScriptPath)
	{
		for (UNiagaraStackModuleItem* ModuleItem : ModuleItems)
		{
			if (ModuleItem == nullptr)
			{
				continue;
			}

			if (UNiagaraScript* FunctionScript = ModuleItem->GetModuleNode().FunctionScript)
			{
				if (FunctionScript->GetPathName() == ScriptPath)
				{
					return ModuleItem;
				}
			}
		}

		return nullptr;
	}

	bool HandleContains(const UNiagaraStackFunctionInput& Input, const TCHAR* Token)
	{
		return Input.GetInputParameterHandle().GetParameterHandleString().ToString().Contains(Token, ESearchCase::IgnoreCase);
	}

	UNiagaraStackFunctionInput* FindInputByHandleToken(UNiagaraStackModuleItem& ModuleItem, const TCHAR* Token)
	{
		TArray<UNiagaraStackFunctionInput*> Inputs;
		ModuleItem.GetParameterInputs(Inputs);

		for (UNiagaraStackFunctionInput* Input : Inputs)
		{
			if (Input != nullptr && HandleContains(*Input, Token))
			{
				return Input;
			}
		}

		return nullptr;
	}

	UNiagaraStackModuleItem* FindAssignmentModule(const TArray<UNiagaraStackModuleItem*>& ModuleItems)
	{
		for (UNiagaraStackModuleItem* ModuleItem : ModuleItems)
		{
			if (ModuleItem == nullptr)
			{
				continue;
			}

			if (const UNiagaraNodeAssignment* AssignmentNode = Cast<UNiagaraNodeAssignment>(&ModuleItem->GetModuleNode()))
			{
				const TArray<FNiagaraVariable>& AssignmentTargets = AssignmentNode->GetAssignmentTargets();
				const bool bHasPosition = AssignmentTargets.ContainsByPredicate([](const FNiagaraVariable& Variable) { return Variable.GetName() == TEXT("Particles.Position"); });
				const bool bHasColor = AssignmentTargets.ContainsByPredicate([](const FNiagaraVariable& Variable) { return Variable.GetName() == TEXT("Particles.Color"); });
				const bool bHasDynamicMaterial = AssignmentTargets.ContainsByPredicate([](const FNiagaraVariable& Variable) { return Variable.GetName() == TEXT("Particles.DynamicMaterialParameter"); });
				const bool bHasSpriteSize = AssignmentTargets.ContainsByPredicate([](const FNiagaraVariable& Variable) { return Variable.GetName() == TEXT("Particles.SpriteSize"); });
				if (bHasPosition && bHasColor && bHasDynamicMaterial && bHasSpriteSize)
				{
					return ModuleItem;
				}
			}
		}

		return nullptr;
	}

	UNiagaraNodeAssignment* AddAssignmentModule(UNiagaraStackModuleItem& InitializeParticleModule)
	{
		TArray<FNiagaraVariable> AssignmentTargets
		{
			INiagaraModule::GetVar_Particles_Position(),
			INiagaraModule::GetVar_Particles_Color(),
			INiagaraModule::GetVar_Particles_DynamicMaterialParameter(),
			INiagaraModule::GetVar_Particles_SpriteSize()
		};

		TArray<FString> DefaultValues
		{
			FNiagaraConstants::GetAttributeDefaultValue(INiagaraModule::GetVar_Particles_Position()),
			FNiagaraConstants::GetAttributeDefaultValue(INiagaraModule::GetVar_Particles_Color()),
			FNiagaraConstants::GetAttributeDefaultValue(INiagaraModule::GetVar_Particles_DynamicMaterialParameter()),
			FNiagaraConstants::GetAttributeDefaultValue(INiagaraModule::GetVar_Particles_SpriteSize())
		};

		return FNiagaraStackGraphUtilities::AddParameterModuleToStack(
			AssignmentTargets,
			*InitializeParticleModule.GetOutputNode(),
			INDEX_NONE,
			DefaultValues);
	}

	bool ConfigureDynamicArrayInput(UNiagaraStackFunctionInput& TargetInput, const TCHAR* DynamicScriptPath, const FNiagaraVariable& ArrayParameter)
	{
		UNiagaraScript* DynamicScript = LoadObject<UNiagaraScript>(nullptr, DynamicScriptPath);
		if (DynamicScript == nullptr)
		{
			return false;
		}

		TargetInput.SetDynamicInput(DynamicScript);

		UNiagaraStackFunctionInput* ArrayInput = nullptr;
		UNiagaraStackFunctionInput* IndexInput = nullptr;
		for (UNiagaraStackFunctionInput* ChildInput : TargetInput.GetChildInputs())
		{
			if (ChildInput == nullptr)
			{
				continue;
			}

			if (ChildInput->GetInputType() == ArrayParameter.GetType()
				|| HandleContains(*ChildInput, TEXT("Array")))
			{
				ArrayInput = ChildInput;
			}
			else if (ChildInput->GetInputType() == FNiagaraTypeDefinition::GetIntDef())
			{
				IndexInput = ChildInput;
			}
		}

		if (ArrayInput == nullptr || IndexInput == nullptr)
		{
			return false;
		}

		ArrayInput->SetLinkedParameterValue(ArrayParameter);
		IndexInput->SetLinkedParameterValue(GetExecIndexParameter());
		return true;
	}

	bool ConfigurePointSpriteMaterial(UMaterial& Material)
	{
		Material.Modify();
		Material.PreEditChange(nullptr);
		Material.MaterialDomain = MD_Surface;
		Material.BlendMode = BLEND_Translucent;
		Material.SetShadingModel(MSM_Unlit);
		Material.TwoSided = true;
		Material.OpacityMaskClipValue = 0.0f;

		bool bNeedsRecompile = false;
		UMaterialEditingLibrary::SetMaterialUsage(&Material, MATUSAGE_NiagaraSprites, bNeedsRecompile);
		UMaterialEditingLibrary::SetMaterialUsage(&Material, MATUSAGE_ParticleSprites, bNeedsRecompile);
		UMaterialEditingLibrary::DeleteAllMaterialExpressions(&Material);

		UMaterialExpressionVertexColor* VertexColor = CreateMaterialExpression<UMaterialExpressionVertexColor>(Material, -600, -200);
		UMaterialExpressionComponentMask* ColorRgb = CreateMaterialExpression<UMaterialExpressionComponentMask>(Material, -420, -240);
		UMaterialExpressionComponentMask* ColorAlpha = CreateMaterialExpression<UMaterialExpressionComponentMask>(Material, -420, -40);
		UMaterialExpressionTextureCoordinate* TextureCoordinate = CreateMaterialExpression<UMaterialExpressionTextureCoordinate>(Material, -600, 100);
		UMaterialExpressionConstant2Vector* Center = CreateMaterialExpression<UMaterialExpressionConstant2Vector>(Material, -600, 260);
		UMaterialExpressionSphereMask* SphereMask = CreateMaterialExpression<UMaterialExpressionSphereMask>(Material, -320, 160);
		UMaterialExpressionMultiply* EmissiveMultiply = CreateMaterialExpression<UMaterialExpressionMultiply>(Material, -60, -120);
		UMaterialExpressionMultiply* OpacityMultiply = CreateMaterialExpression<UMaterialExpressionMultiply>(Material, -60, 120);

		if (VertexColor == nullptr
			|| ColorRgb == nullptr
			|| ColorAlpha == nullptr
			|| TextureCoordinate == nullptr
			|| Center == nullptr
			|| SphereMask == nullptr
			|| EmissiveMultiply == nullptr
			|| OpacityMultiply == nullptr)
		{
			Material.PostEditChange();
			return false;
		}

		TextureCoordinate->CoordinateIndex = 0;
		ColorRgb->R = true;
		ColorRgb->G = true;
		ColorRgb->B = true;
		ColorAlpha->A = true;
		Center->R = 0.5f;
		Center->G = 0.5f;
		SphereMask->AttenuationRadius = 0.48f;
		SphereMask->HardnessPercent = 8.0f;

		UMaterialEditingLibrary::ConnectMaterialExpressions(VertexColor, FString(), ColorRgb, TEXT("Input"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(VertexColor, FString(), ColorAlpha, TEXT("Input"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(TextureCoordinate, FString(), SphereMask, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(Center, FString(), SphereMask, TEXT("B"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(ColorRgb, FString(), EmissiveMultiply, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(ColorAlpha, FString(), OpacityMultiply, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(SphereMask, FString(), OpacityMultiply, TEXT("B"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(OpacityMultiply, FString(), EmissiveMultiply, TEXT("B"));

		UMaterialEditingLibrary::ConnectMaterialProperty(EmissiveMultiply, FString(), MP_EmissiveColor);
		UMaterialEditingLibrary::ConnectMaterialProperty(OpacityMultiply, FString(), MP_Opacity);
		UMaterialEditingLibrary::LayoutMaterialExpressions(&Material);
		UMaterialEditingLibrary::RecompileMaterial(&Material);

		Material.PostEditChange();
		Material.MarkPackageDirty();
		return true;
	}

	UMaterialInterface* FindOrCreatePointSpriteMaterial(const FString& PackagePath)
	{
		const FString AssetName = PointSpriteMaterialAssetName;
		const FString ObjectPath = MakeObjectPath(PackagePath, AssetName);

		UMaterial* SpriteMaterial = LoadObject<UMaterial>(nullptr, *ObjectPath);
		if (SpriteMaterial == nullptr)
		{
			UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
			FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
			SpriteMaterial = Cast<UMaterial>(AssetToolsModule.Get().CreateAsset(AssetName, PackagePath, UMaterial::StaticClass(), Factory));
		}

		if (SpriteMaterial != nullptr && !ConfigurePointSpriteMaterial(*SpriteMaterial))
		{
			return nullptr;
		}

		return SpriteMaterial;
	}

	bool ConfigureSpriteRenderer(FVersionedNiagaraEmitter& Emitter, UMaterialInterface* SpriteMaterial)
	{
		FVersionedNiagaraEmitterData* EmitterData = Emitter.GetEmitterData();
		if (EmitterData == nullptr)
		{
			return false;
		}

		UMaterialInterface* DefaultSpriteMaterial = LoadObject<UMaterialInterface>(nullptr, DefaultSpriteMaterialPath);
		UMaterialInterface* EffectiveSpriteMaterial = SpriteMaterial != nullptr ? SpriteMaterial : DefaultSpriteMaterial;
		const FVersionedNiagaraEmitterBase EmitterBase = Emitter.ToBase();
		bool bConfiguredSpriteRenderer = false;

		for (UNiagaraRendererProperties* RendererProperties : EmitterData->GetRenderers())
		{
			UNiagaraSpriteRendererProperties* SpriteRenderer = Cast<UNiagaraSpriteRendererProperties>(RendererProperties);
			if (SpriteRenderer == nullptr)
			{
				continue;
			}

			SpriteRenderer->Modify();
			if (EffectiveSpriteMaterial != nullptr)
			{
				SpriteRenderer->Material = EffectiveSpriteMaterial;
			}

#if WITH_EDITORONLY_DATA
			SpriteRenderer->bIncludeInHitProxy = false;
#endif

			SpriteRenderer->PositionBinding.SetValue(INiagaraModule::GetVar_Particles_Position().GetName(), EmitterBase, ENiagaraRendererSourceDataMode::Particles);
			SpriteRenderer->ColorBinding.SetValue(INiagaraModule::GetVar_Particles_Color().GetName(), EmitterBase, ENiagaraRendererSourceDataMode::Particles);
			SpriteRenderer->DynamicMaterialBinding.SetValue(INiagaraModule::GetVar_Particles_DynamicMaterialParameter().GetName(), EmitterBase, ENiagaraRendererSourceDataMode::Particles);
			SpriteRenderer->SpriteSizeBinding.SetValue(INiagaraModule::GetVar_Particles_SpriteSize().GetName(), EmitterBase, ENiagaraRendererSourceDataMode::Particles);
			SpriteRenderer->SortMode = ENiagaraSortMode::ViewDepth;
			SpriteRenderer->SortPrecision = ENiagaraRendererSortPrecision::High;
			SpriteRenderer->bSortOnlyWhenTranslucent = true;
			bConfiguredSpriteRenderer = true;
		}

		return bConfiguredSpriteRenderer;
	}

	bool ConfigurePointCloudNiagaraSystem(UNiagaraSystem& System, UMaterialInterface* SpriteMaterial)
	{
		System.Modify();

		float DefaultLifetime = SpzNiagaraParameters::DefaultParticleLifetimeSeconds;
		EnsureUserParameter(System, GetPointCountParameter());
		EnsureUserParameter(System, GetPositionsParameter());
		EnsureUserParameter(System, GetColorsParameter());
		EnsureUserParameter(System, GetDynamicMaterialParametersParameter());
		EnsureUserParameter(System, GetSpriteSizesParameter());
		EnsureUserParameter(System, GetLifetimeParameter(), reinterpret_cast<const uint8*>(&DefaultLifetime));

		TSharedPtr<FNiagaraSystemViewModel> SystemViewModel = CreateSystemViewModel(System);
		if (!SystemViewModel.IsValid() || SystemViewModel->GetEmitterHandleViewModels().Num() == 0)
		{
			return false;
		}

		FVersionedNiagaraEmitter Emitter = SystemViewModel->GetEmitterHandleViewModels()[0]->GetEmitterViewModel()->GetEmitter();
		if (FVersionedNiagaraEmitterData* EmitterData = Emitter.GetEmitterData())
		{
			EmitterData->bLocalSpace = true;
		}
		else
		{
			return false;
		}

		if (!ConfigureSpriteRenderer(Emitter, SpriteMaterial))
		{
			return false;
		}

		TArray<UNiagaraStackModuleItem*> ModuleItems = GetAllModuleItems(SystemViewModel);
		UNiagaraStackModuleItem* BurstModule = FindModuleItemByScriptPath(ModuleItems, SpawnBurstModulePath);
		UNiagaraStackModuleItem* InitializeParticleModule = FindModuleItemByScriptPath(ModuleItems, InitializeParticleModulePath);
		UNiagaraStackModuleItem* ParticleStateModule = FindModuleItemByScriptPath(ModuleItems, ParticleStateModulePath);

		if (BurstModule == nullptr || InitializeParticleModule == nullptr)
		{
			return false;
		}

		if (ParticleStateModule != nullptr)
		{
			ParticleStateModule->SetEnabled(false);
		}

		if (UNiagaraStackFunctionInput* SpawnCountInput = FindInputByHandleToken(*BurstModule, TEXT("Spawn Count")))
		{
			SpawnCountInput->SetLinkedParameterValue(GetPointCountParameter());
		}
		else
		{
			return false;
		}

		UNiagaraStackModuleItem* AssignmentModule = FindAssignmentModule(ModuleItems);
		if (AssignmentModule == nullptr)
		{
			if (AddAssignmentModule(*InitializeParticleModule) == nullptr)
			{
				return false;
			}

			SystemViewModel = CreateSystemViewModel(System);
			ModuleItems = GetAllModuleItems(SystemViewModel);
			AssignmentModule = FindAssignmentModule(ModuleItems);
			if (AssignmentModule == nullptr)
			{
				return false;
			}
		}

		UNiagaraStackFunctionInput* PositionInput = FindInputByHandleToken(*AssignmentModule, TEXT("Particles.Position"));
		UNiagaraStackFunctionInput* ColorInput = FindInputByHandleToken(*AssignmentModule, TEXT("Particles.Color"));
		UNiagaraStackFunctionInput* DynamicMaterialInput = FindInputByHandleToken(*AssignmentModule, TEXT("Particles.DynamicMaterialParameter"));
		UNiagaraStackFunctionInput* SpriteSizeInput = FindInputByHandleToken(*AssignmentModule, TEXT("Particles.SpriteSize"));

		if (PositionInput == nullptr || ColorInput == nullptr || DynamicMaterialInput == nullptr || SpriteSizeInput == nullptr)
		{
			return false;
		}

		const bool bConfiguredPosition = ConfigureDynamicArrayInput(*PositionInput, SelectPositionFromArrayPath, GetPositionsParameter());
		const bool bConfiguredColor = ConfigureDynamicArrayInput(*ColorInput, SelectColorFromArrayPath, GetColorsParameter());
		const bool bConfiguredDynamicMaterial = ConfigureDynamicArrayInput(*DynamicMaterialInput, SelectVector4FromArrayPath, GetDynamicMaterialParametersParameter());
		const bool bConfiguredSpriteSize = ConfigureDynamicArrayInput(*SpriteSizeInput, SelectVector2FromArrayPath, GetSpriteSizesParameter());

		if (!bConfiguredPosition || !bConfiguredColor || !bConfiguredDynamicMaterial || !bConfiguredSpriteSize)
		{
			return false;
		}

		System.RequestCompile(false);
		System.MarkPackageDirty();
		return true;
	}

	UNiagaraSystem* FindOrCreateNiagaraSystem(const FString& PackagePath, UMaterialInterface* SpriteMaterial)
	{
		const FString AssetName = NiagaraSystemAssetName;
		const FString ObjectPath = MakeObjectPath(PackagePath, AssetName);

		UNiagaraSystem* NiagaraSystem = LoadObject<UNiagaraSystem>(nullptr, *ObjectPath);
		if (NiagaraSystem == nullptr)
		{
			UNiagaraEmitter* TemplateEmitter = LoadObject<UNiagaraEmitter>(nullptr, SimpleSpriteBurstEmitterPath);
			if (TemplateEmitter == nullptr)
			{
				return nullptr;
			}

			UNiagaraSystemFactoryNew* Factory = NewObject<UNiagaraSystemFactoryNew>();
			Factory->EmittersToAddToNewSystem.Add(FVersionedNiagaraEmitter(TemplateEmitter, TemplateEmitter->GetExposedVersion().VersionGuid));

			FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
			NiagaraSystem = Cast<UNiagaraSystem>(AssetToolsModule.Get().CreateAsset(AssetName, PackagePath, UNiagaraSystem::StaticClass(), Factory));
		}

		if (NiagaraSystem != nullptr)
		{
			ConfigurePointCloudNiagaraSystem(*NiagaraSystem, SpriteMaterial);
		}

		return NiagaraSystem;
	}

	void UpdateBlueprintDefaults(UBlueprint& Blueprint, USpzPointCloudAsset& PointCloudAsset, UMaterialInterface& SplatMaterial)
	{
		FKismetEditorUtilities::CompileBlueprint(&Blueprint);

		if (Blueprint.GeneratedClass == nullptr)
		{
			return;
		}

		ASpzNiagaraPointCloudActor* DefaultActor = Cast<ASpzNiagaraPointCloudActor>(Blueprint.GeneratedClass->GetDefaultObject());
		if (DefaultActor == nullptr)
		{
			return;
		}

		DefaultActor->Modify();
		DefaultActor->PointCloudAsset = &PointCloudAsset;
		DefaultActor->NiagaraSystemAsset = nullptr;
		DefaultActor->SplatMaterial = &SplatMaterial;
		DefaultActor->MaxRenderPoints = FMath::Min(SpzNiagaraParameters::DefaultMaxRenderPoints, PointCloudAsset.GetStoredPointCount());
		DefaultActor->SpriteSizeMultiplier = 3.0f;
		DefaultActor->ParticleLifetimeSeconds = SpzNiagaraParameters::DefaultParticleLifetimeSeconds;
		DefaultActor->RenderRotationOffset = FRotator(-90.0f, 0.0f, 0.0f);
		DefaultActor->bRefreshInConstructionScript = true;

		FBlueprintEditorUtils::MarkBlueprintAsModified(&Blueprint);
		Blueprint.MarkPackageDirty();
		FKismetEditorUtilities::CompileBlueprint(&Blueprint);
	}

	UBlueprint* FindOrCreateBlueprint(USpzPointCloudAsset& PointCloudAsset, UMaterialInterface& SplatMaterial)
	{
		const FString PackagePath = FPackageName::GetLongPackagePath(PointCloudAsset.GetOutermost()->GetName());
		const FString AssetName = FString::Printf(TEXT("%s%s"), NiagaraBlueprintPrefix, *PointCloudAsset.GetName());
		const FString ObjectPath = MakeObjectPath(PackagePath, AssetName);

		UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *ObjectPath);
		if (Blueprint == nullptr)
		{
			UPackage* Package = CreatePackage(*FString::Printf(TEXT("%s/%s"), *PackagePath, *AssetName));
			Blueprint = FKismetEditorUtilities::CreateBlueprint(
				ASpzNiagaraPointCloudActor::StaticClass(),
				Package,
				FName(*AssetName),
				BPTYPE_Normal,
				UBlueprint::StaticClass(),
				UBlueprintGeneratedClass::StaticClass(),
				FName(TEXT("SPZImport")));

			if (Blueprint != nullptr)
			{
				FAssetRegistryModule::AssetCreated(Blueprint);
			}
		}

		if (Blueprint != nullptr)
		{
			UpdateBlueprintDefaults(*Blueprint, PointCloudAsset, SplatMaterial);
		}

		return Blueprint;
	}
}

void FSpzEditorUtilities::GeneratePointCloudSupportAssets(USpzPointCloudAsset& PointCloudAsset)
{
	const FString PackagePath = FPackageName::GetLongPackagePath(PointCloudAsset.GetOutermost()->GetName());
	UMaterialInterface* SpriteMaterial = FindOrCreatePointSpriteMaterial(PackagePath);
	if (SpriteMaterial == nullptr)
	{
		return;
	}

	FindOrCreateBlueprint(PointCloudAsset, *SpriteMaterial);
}
