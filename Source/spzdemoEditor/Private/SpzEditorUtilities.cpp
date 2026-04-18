// Copyright Epic Games, Inc. All Rights Reserved.

#include "SpzEditorUtilities.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "NiagaraConstants.h"
#include "NiagaraDataInterfaceArrayFloat.h"
#include "NiagaraEditorUtilities.h"
#include "NiagaraEmitter.h"
#include "NiagaraModule.h"
#include "NiagaraNodeAssignment.h"
#include "NiagaraSystem.h"
#include "NiagaraSystemEditorData.h"
#include "NiagaraSystemFactoryNew.h"
#include "NiagaraValidationRules.h"
#include "SpzNiagaraParameters.h"
#include "SpzNiagaraPointCloudActor.h"
#include "SpzPointCloudAsset.h"
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
	constexpr TCHAR NiagaraBlueprintPrefix[] = TEXT("BP_");

	constexpr TCHAR SimpleSpriteBurstEmitterPath[] = TEXT("/Niagara/DefaultAssets/Templates/Emitters/SimpleSpriteBurst.SimpleSpriteBurst");
	constexpr TCHAR SpawnBurstModulePath[] = TEXT("/Niagara/Modules/Emitter/SpawnBurst_Instantaneous.SpawnBurst_Instantaneous");
	constexpr TCHAR InitializeParticleModulePath[] = TEXT("/Niagara/Modules/Spawn/Initialization/V2/InitializeParticle.InitializeParticle");
	constexpr TCHAR ParticleStateModulePath[] = TEXT("/Niagara/Modules/Update/Lifetime/ParticleState.ParticleState");
	constexpr TCHAR SelectPositionFromArrayPath[] = TEXT("/Niagara/DynamicInputs/Arrays/SelectPositionFromArray.SelectPositionFromArray");
	constexpr TCHAR SelectColorFromArrayPath[] = TEXT("/Niagara/DynamicInputs/Arrays/SelectLinearColorFromArray.SelectLinearColorFromArray");
	constexpr TCHAR SelectVector2FromArrayPath[] = TEXT("/Niagara/DynamicInputs/Arrays/SelectVector2DFromArray.SelectVector2DFromArray");

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

	FNiagaraVariable GetSpriteSizesParameter()
	{
		static const FNiagaraVariable Variable = MakeUserParameter(FNiagaraTypeDefinition(UNiagaraDataInterfaceArrayFloat2::StaticClass()), SpzNiagaraParameters::SpriteSizes);
		return Variable;
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
				const bool bHasSpriteSize = AssignmentTargets.ContainsByPredicate([](const FNiagaraVariable& Variable) { return Variable.GetName() == TEXT("Particles.SpriteSize"); });
				if (bHasPosition && bHasColor && bHasSpriteSize)
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
			INiagaraModule::GetVar_Particles_SpriteSize()
		};

		TArray<FString> DefaultValues
		{
			FNiagaraConstants::GetAttributeDefaultValue(INiagaraModule::GetVar_Particles_Position()),
			FNiagaraConstants::GetAttributeDefaultValue(INiagaraModule::GetVar_Particles_Color()),
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

	bool ConfigurePointCloudNiagaraSystem(UNiagaraSystem& System)
	{
		System.Modify();

		float DefaultLifetime = SpzNiagaraParameters::DefaultParticleLifetimeSeconds;
		EnsureUserParameter(System, GetPointCountParameter());
		EnsureUserParameter(System, GetPositionsParameter());
		EnsureUserParameter(System, GetColorsParameter());
		EnsureUserParameter(System, GetSpriteSizesParameter());
		EnsureUserParameter(System, GetLifetimeParameter(), reinterpret_cast<const uint8*>(&DefaultLifetime));

		TSharedPtr<FNiagaraSystemViewModel> SystemViewModel = CreateSystemViewModel(System);
		if (!SystemViewModel.IsValid() || SystemViewModel->GetEmitterHandleViewModels().Num() == 0)
		{
			return false;
		}

		if (FVersionedNiagaraEmitterData* EmitterData = SystemViewModel->GetEmitterHandleViewModels()[0]->GetEmitterViewModel()->GetEmitter().GetEmitterData())
		{
			EmitterData->bLocalSpace = true;
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
		UNiagaraStackFunctionInput* SpriteSizeInput = FindInputByHandleToken(*AssignmentModule, TEXT("Particles.SpriteSize"));

		if (PositionInput == nullptr || ColorInput == nullptr || SpriteSizeInput == nullptr)
		{
			return false;
		}

		const bool bConfiguredPosition = ConfigureDynamicArrayInput(*PositionInput, SelectPositionFromArrayPath, GetPositionsParameter());
		const bool bConfiguredColor = ConfigureDynamicArrayInput(*ColorInput, SelectColorFromArrayPath, GetColorsParameter());
		const bool bConfiguredSpriteSize = ConfigureDynamicArrayInput(*SpriteSizeInput, SelectVector2FromArrayPath, GetSpriteSizesParameter());

		if (!bConfiguredPosition || !bConfiguredColor || !bConfiguredSpriteSize)
		{
			return false;
		}

		System.RequestCompile(false);
		System.MarkPackageDirty();
		return true;
	}

	UNiagaraSystem* FindOrCreateNiagaraSystem(const FString& PackagePath)
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
			ConfigurePointCloudNiagaraSystem(*NiagaraSystem);
		}

		return NiagaraSystem;
	}

	void UpdateBlueprintDefaults(UBlueprint& Blueprint, USpzPointCloudAsset& PointCloudAsset, UNiagaraSystem& NiagaraSystem)
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
		DefaultActor->NiagaraSystemAsset = &NiagaraSystem;
		DefaultActor->MaxRenderPoints = FMath::Min(SpzNiagaraParameters::DefaultMaxRenderPoints, PointCloudAsset.GetStoredPointCount());
		DefaultActor->SpriteSizeMultiplier = 1.0f;
		DefaultActor->ParticleLifetimeSeconds = SpzNiagaraParameters::DefaultParticleLifetimeSeconds;

		FBlueprintEditorUtils::MarkBlueprintAsModified(&Blueprint);
		Blueprint.MarkPackageDirty();
		FKismetEditorUtilities::CompileBlueprint(&Blueprint);
	}

	UBlueprint* FindOrCreateBlueprint(USpzPointCloudAsset& PointCloudAsset, UNiagaraSystem& NiagaraSystem)
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
			UpdateBlueprintDefaults(*Blueprint, PointCloudAsset, NiagaraSystem);
		}

		return Blueprint;
	}
}

void FSpzEditorUtilities::GeneratePointCloudSupportAssets(USpzPointCloudAsset& PointCloudAsset)
{
	const FString PackagePath = FPackageName::GetLongPackagePath(PointCloudAsset.GetOutermost()->GetName());
	UNiagaraSystem* NiagaraSystem = FindOrCreateNiagaraSystem(PackagePath);
	if (NiagaraSystem == nullptr)
	{
		return;
	}

	FindOrCreateBlueprint(PointCloudAsset, *NiagaraSystem);
}
