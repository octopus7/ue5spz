#include "SpzGaussianActorRebuildEditorUtilities.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Factories/MaterialFactoryNew.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "MaterialEditingLibrary.h"
#include "Modules/ModuleManager.h"
#include "SpzGaussianActorRebuildActor.h"
#include "SpzGaussianActorRebuildAsset.h"
#include "UObject/Package.h"

#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionConstant2Vector.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionSphereMask.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionVertexColor.h"

namespace
{
	constexpr TCHAR SharedRootPath[] = TEXT("/Game/SPZ_GAR/_Shared");
	constexpr TCHAR UnlitMaterialAssetName[] = TEXT("M_GAR_GaussianUnlit");
	constexpr TCHAR RelightMaterialAssetName[] = TEXT("M_GAR_GaussianRelight");
	constexpr TCHAR BlueprintPrefix[] = TEXT("BP_GAR_");

	FString MakeObjectPath(const FString& PackagePath, const FString& AssetName)
	{
		return FString::Printf(TEXT("%s/%s.%s"), *PackagePath, *AssetName, *AssetName);
	}

	FString MakePackageName(const FString& PackagePath, const FString& AssetName)
	{
		return FString::Printf(TEXT("%s/%s"), *PackagePath, *AssetName);
	}

	template <typename TAsset, typename TFactory>
	TAsset* FindOrCreateAsset(const FString& PackagePath, const FString& AssetName)
	{
		if (TAsset* ExistingAsset = LoadObject<TAsset>(nullptr, *MakeObjectPath(PackagePath, AssetName)))
		{
			return ExistingAsset;
		}

		TFactory* Factory = NewObject<TFactory>();
		FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
		return Cast<TAsset>(AssetToolsModule.Get().CreateAsset(AssetName, PackagePath, TAsset::StaticClass(), Factory));
	}

	template <typename TExpression>
	TExpression* CreateMaterialExpression(UMaterial& Material, int32 NodePosX, int32 NodePosY)
	{
		return Cast<TExpression>(UMaterialEditingLibrary::CreateMaterialExpression(&Material, TExpression::StaticClass(), NodePosX, NodePosY));
	}

	bool ConfigureGaussianMaterial(UMaterial& Material, bool bRelight)
	{
		Material.Modify();
		Material.PreEditChange(nullptr);
		Material.MaterialDomain = MD_Surface;
		Material.BlendMode = BLEND_Translucent;
		Material.SetShadingModel(bRelight ? MSM_DefaultLit : MSM_Unlit);
		Material.TwoSided = true;
		Material.OpacityMaskClipValue = 0.0f;

		UMaterialEditingLibrary::DeleteAllMaterialExpressions(&Material);

		UMaterialExpressionVertexColor* VertexColor = CreateMaterialExpression<UMaterialExpressionVertexColor>(Material, -620, -180);
		UMaterialExpressionComponentMask* ColorRgb = CreateMaterialExpression<UMaterialExpressionComponentMask>(Material, -420, -220);
		UMaterialExpressionComponentMask* ColorAlpha = CreateMaterialExpression<UMaterialExpressionComponentMask>(Material, -420, -20);
		UMaterialExpressionTextureCoordinate* TextureCoordinate = CreateMaterialExpression<UMaterialExpressionTextureCoordinate>(Material, -620, 120);
		UMaterialExpressionConstant2Vector* Center = CreateMaterialExpression<UMaterialExpressionConstant2Vector>(Material, -620, 280);
		UMaterialExpressionSphereMask* SphereMask = CreateMaterialExpression<UMaterialExpressionSphereMask>(Material, -340, 180);
		UMaterialExpressionMultiply* OpacityMultiply = CreateMaterialExpression<UMaterialExpressionMultiply>(Material, -80, 80);
		UMaterialExpressionMultiply* ColorMultiply = CreateMaterialExpression<UMaterialExpressionMultiply>(Material, -80, -140);

		if (VertexColor == nullptr
			|| ColorRgb == nullptr
			|| ColorAlpha == nullptr
			|| TextureCoordinate == nullptr
			|| Center == nullptr
			|| SphereMask == nullptr
			|| OpacityMultiply == nullptr
			|| ColorMultiply == nullptr)
		{
			Material.PostEditChange();
			return false;
		}

		ColorRgb->R = true;
		ColorRgb->G = true;
		ColorRgb->B = true;
		ColorAlpha->A = true;
		TextureCoordinate->CoordinateIndex = 0;
		Center->R = 0.5f;
		Center->G = 0.5f;
		SphereMask->AttenuationRadius = 0.48f;
		SphereMask->HardnessPercent = 8.0f;

		UMaterialEditingLibrary::ConnectMaterialExpressions(VertexColor, FString(), ColorRgb, TEXT("Input"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(VertexColor, FString(), ColorAlpha, TEXT("Input"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(TextureCoordinate, FString(), SphereMask, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(Center, FString(), SphereMask, TEXT("B"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(ColorAlpha, FString(), OpacityMultiply, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(SphereMask, FString(), OpacityMultiply, TEXT("B"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(ColorRgb, FString(), ColorMultiply, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(OpacityMultiply, FString(), ColorMultiply, TEXT("B"));

		if (bRelight)
		{
			UMaterialEditingLibrary::ConnectMaterialProperty(ColorRgb, FString(), MP_BaseColor);
		}
		else
		{
			UMaterialEditingLibrary::ConnectMaterialProperty(ColorMultiply, FString(), MP_EmissiveColor);
		}

		UMaterialEditingLibrary::ConnectMaterialProperty(OpacityMultiply, FString(), MP_Opacity);
		UMaterialEditingLibrary::LayoutMaterialExpressions(&Material);
		UMaterialEditingLibrary::RecompileMaterial(&Material);

		Material.PostEditChange();
		Material.MarkPackageDirty();
		return true;
	}

	UMaterialInterface* FindOrCreateMaterial(const FString& PackagePath, const FString& AssetName, bool bRelight)
	{
		UMaterial* Material = FindOrCreateAsset<UMaterial, UMaterialFactoryNew>(PackagePath, AssetName);
		if (Material == nullptr)
		{
			return nullptr;
		}

		return ConfigureGaussianMaterial(*Material, bRelight) ? Material : nullptr;
	}

	void UpdateBlueprintDefaults(UBlueprint& Blueprint, USpzGaussianActorRebuildAsset& ImportedAsset)
	{
		FKismetEditorUtilities::CompileBlueprint(&Blueprint);
		if (Blueprint.GeneratedClass == nullptr)
		{
			return;
		}

		ASpzGaussianActorRebuildActor* DefaultActor = Cast<ASpzGaussianActorRebuildActor>(Blueprint.GeneratedClass->GetDefaultObject());
		if (DefaultActor == nullptr)
		{
			return;
		}

		DefaultActor->Modify();
		DefaultActor->ImportedAsset = &ImportedAsset;
		DefaultActor->UnlitMaterial = ImportedAsset.UnlitMaterial;
		DefaultActor->RelightMaterial = ImportedAsset.RelightMaterial;
		DefaultActor->GaussianSpriteScale = 1.0f;
		DefaultActor->AlbedoTint = FLinearColor::White;
		DefaultActor->UseRelighting = false;
		DefaultActor->MaxRenderPoints = 0;

		FBlueprintEditorUtils::MarkBlueprintAsModified(&Blueprint);
		Blueprint.MarkPackageDirty();
		FKismetEditorUtilities::CompileBlueprint(&Blueprint);
	}

	UBlueprint* FindOrCreateBlueprint(USpzGaussianActorRebuildAsset& ImportedAsset)
	{
		if (ImportedAsset.GeneratedAssetRoot.IsEmpty())
		{
			return nullptr;
		}

		const FString AssetName = FString::Printf(TEXT("%s%s"), BlueprintPrefix, *ImportedAsset.GetName());
		if (UBlueprint* ExistingBlueprint = LoadObject<UBlueprint>(nullptr, *MakeObjectPath(ImportedAsset.GeneratedAssetRoot, AssetName)))
		{
			UpdateBlueprintDefaults(*ExistingBlueprint, ImportedAsset);
			return ExistingBlueprint;
		}

		UPackage* Package = CreatePackage(*MakePackageName(ImportedAsset.GeneratedAssetRoot, AssetName));
		if (Package == nullptr)
		{
			return nullptr;
		}

		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
			ASpzGaussianActorRebuildActor::StaticClass(),
			Package,
			FName(*AssetName),
			BPTYPE_Normal,
			UBlueprint::StaticClass(),
			UBlueprintGeneratedClass::StaticClass(),
			FName(TEXT("SpzGaussianActorRebuildImport")));
		if (Blueprint == nullptr)
		{
			return nullptr;
		}

		FAssetRegistryModule::AssetCreated(Blueprint);
		UpdateBlueprintDefaults(*Blueprint, ImportedAsset);
		return Blueprint;
	}
}

bool SpzGaussianActorRebuildEditorUtilities::GenerateSupportAssets(USpzGaussianActorRebuildAsset& ImportedAsset, FString& OutErrorMessage)
{
	OutErrorMessage.Reset();

	UMaterialInterface* UnlitMaterial = FindOrCreateMaterial(SharedRootPath, UnlitMaterialAssetName, false);
	if (UnlitMaterial == nullptr)
	{
		OutErrorMessage = TEXT("Failed to create SPZ Gaussian Actor Rebuild unlit material.");
		return false;
	}

	UMaterialInterface* RelightMaterial = FindOrCreateMaterial(SharedRootPath, RelightMaterialAssetName, true);
	if (RelightMaterial == nullptr)
	{
		OutErrorMessage = TEXT("Failed to create SPZ Gaussian Actor Rebuild relight material.");
		return false;
	}

	ImportedAsset.Modify();
	ImportedAsset.UnlitMaterial = UnlitMaterial;
	ImportedAsset.RelightMaterial = RelightMaterial;

	UBlueprint* ActorBlueprint = FindOrCreateBlueprint(ImportedAsset);
	if (ActorBlueprint == nullptr)
	{
		OutErrorMessage = TEXT("Failed to create SPZ Gaussian Actor Rebuild blueprint actor.");
		return false;
	}

	ImportedAsset.ActorBlueprint = ActorBlueprint;
	ImportedAsset.MarkPackageDirty();
	return true;
}
