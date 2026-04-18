#include "SpzGaussianActorRebuildFactory.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/FeedbackContext.h"
#include "Misc/Paths.h"
#include "SpzGaussianActorRebuildAsset.h"
#include "SpzGaussianActorRebuildEditorUtilities.h"

#include "Engine/Texture2D.h"

#include "load-spz.h"

namespace
{
	constexpr float SpzUnitsToCentimeters = 100.0f;

	float Sigmoid(float Value)
	{
		return 1.0f / (1.0f + FMath::Exp(-Value));
	}

	float DecodeColorChannel(float Value)
	{
		return FMath::Clamp(0.5f + (0.282095f * Value), 0.0f, 1.0f);
	}

	FVector3f ConvertSpzDirectionToUnreal(const FVector3f& DirectionData)
	{
		return FVector3f(-DirectionData.Z, DirectionData.X, DirectionData.Y);
	}

	FVector3f ConvertSpzPositionToUnreal(const float* PositionData)
	{
		return ConvertSpzDirectionToUnreal(FVector3f(PositionData[0], PositionData[1], PositionData[2]) * SpzUnitsToCentimeters);
	}

	FQuat4f ConvertSpzQuaternionToUnreal(const float* RotationData)
	{
		const FQuat4f Rotation(RotationData[0], RotationData[1], RotationData[2], RotationData[3]);
		const FQuat4f Basis(FVector3f::XAxisVector, -HALF_PI);
		return (Basis * Rotation).GetNormalized();
	}

	FVector3f ConvertSpzScaleToUnreal(const float* ScaleData)
	{
		return FVector3f(
			FMath::Exp(ScaleData[0]) * SpzUnitsToCentimeters,
			FMath::Exp(ScaleData[1]) * SpzUnitsToCentimeters,
			FMath::Exp(ScaleData[2]) * SpzUnitsToCentimeters);
	}

	void ConvertSpzAxesToUnreal(const float* RotationData, const float* ScaleData, FVector3f& OutAxisX, FVector3f& OutAxisY, FVector3f& OutAxisZ)
	{
		const FQuat4f Rotation = FQuat4f(RotationData[0], RotationData[1], RotationData[2], RotationData[3]).GetNormalized();
		const FVector3f ScaleX(FMath::Exp(ScaleData[0]) * SpzUnitsToCentimeters, 0.0f, 0.0f);
		const FVector3f ScaleY(0.0f, FMath::Exp(ScaleData[1]) * SpzUnitsToCentimeters, 0.0f);
		const FVector3f ScaleZ(0.0f, 0.0f, FMath::Exp(ScaleData[2]) * SpzUnitsToCentimeters);

		OutAxisX = ConvertSpzDirectionToUnreal(Rotation.RotateVector(ScaleX));
		OutAxisY = ConvertSpzDirectionToUnreal(Rotation.RotateVector(ScaleY));
		OutAxisZ = ConvertSpzDirectionToUnreal(Rotation.RotateVector(ScaleZ));
	}

	FLinearColor ConvertSpzSH0ToUnreal(const float* ColorData)
	{
		return FLinearColor(
			DecodeColorChannel(ColorData[0]),
			DecodeColorChannel(ColorData[1]),
			DecodeColorChannel(ColorData[2]),
			1.0f);
	}

	FString MakeObjectPath(const FString& PackagePath, const FString& AssetName)
	{
		return FString::Printf(TEXT("%s/%s.%s"), *PackagePath, *AssetName, *AssetName);
	}

	FString BuildGeneratedAssetRoot(const FString& PackageName, const FString& AssetName)
	{
		const uint32 Hash = FCrc::StrCrc32(*PackageName);
		return FString::Printf(TEXT("/Game/SPZ_GAR/Generated/%s_%08X"), *AssetName, Hash);
	}

	UTexture2D* CreateOrUpdatePackedTexture(
		const FString& PackagePath,
		const FString& AssetName,
		int32 Width,
		int32 Height,
		const TArray<FFloat16Color>& Pixels)
	{
		const FString ObjectPath = MakeObjectPath(PackagePath, AssetName);
		UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, *ObjectPath);
		if (Texture == nullptr)
		{
			UPackage* Package = CreatePackage(*FString::Printf(TEXT("%s/%s"), *PackagePath, *AssetName));
			Texture = NewObject<UTexture2D>(Package, *AssetName, RF_Public | RF_Standalone | RF_Transactional);
			FAssetRegistryModule::AssetCreated(Texture);
		}

		if (Texture == nullptr)
		{
			return nullptr;
		}

		Texture->Modify();
		Texture->MipGenSettings = TMGS_NoMipmaps;
		Texture->CompressionNone = true;
		Texture->CompressionSettings = TC_HDR;
		Texture->SRGB = false;
		Texture->NeverStream = true;
		Texture->Filter = TF_Nearest;
		Texture->AddressX = TA_Clamp;
		Texture->AddressY = TA_Clamp;
		Texture->LODGroup = TEXTUREGROUP_Pixels2D;
		Texture->Source.Init(Width, Height, 1, 1, TSF_RGBA16F, reinterpret_cast<const uint8*>(Pixels.GetData()));
		Texture->PostEditChange();
		Texture->MarkPackageDirty();
		return Texture;
	}
}

USpzGaussianActorRebuildFactory::USpzGaussianActorRebuildFactory()
{
	SupportedClass = USpzGaussianActorRebuildAsset::StaticClass();
	bCreateNew = false;
	bEditorImport = true;
	bText = false;
	Formats.Add(TEXT("spz;SPZ Gaussian actor rebuild"));
	ImportPriority = DefaultImportPriority + 25;
}

bool USpzGaussianActorRebuildFactory::FactoryCanImport(const FString& Filename)
{
	return FPaths::GetExtension(Filename).Equals(TEXT("spz"), ESearchCase::IgnoreCase);
}

UObject* USpzGaussianActorRebuildFactory::FactoryCreateFile(
	UClass* InClass,
	UObject* InParent,
	FName InName,
	EObjectFlags Flags,
	const FString& Filename,
	const TCHAR* Parms,
	FFeedbackContext* Warn,
	bool& bOutOperationCanceled)
{
	bOutOperationCanceled = false;

	spz::UnpackOptions UnpackOptions;
	UnpackOptions.to = spz::CoordinateSystem::RUB;

	spz::GaussianCloud GaussianCloud = spz::loadSpz(TCHAR_TO_UTF8(*Filename), UnpackOptions);
	if (GaussianCloud.numPoints <= 0 || GaussianCloud.positions.size() / 3 != static_cast<size_t>(GaussianCloud.numPoints))
	{
		if (Warn != nullptr)
		{
			Warn->Logf(TEXT("SPZ Gaussian actor rebuild import failed for %s"), *Filename);
		}
		return nullptr;
	}

	const int32 PointCount = GaussianCloud.numPoints;
	const int32 TextureWidth = FMath::Max(1, FMath::CeilToInt(FMath::Sqrt(static_cast<float>(PointCount))));
	const int32 TextureHeight = FMath::Max(1, FMath::DivideAndRoundUp(PointCount, TextureWidth));
	const int32 TexturePixelCount = TextureWidth * TextureHeight;

	TArray<FFloat16Color> PositionPixels;
	TArray<FFloat16Color> QuatPixels;
	TArray<FFloat16Color> ScaleAPixels;
	TArray<FFloat16Color> SH0Pixels;
	TArray<FVector3f> Positions;
	TArray<FLinearColor> Colors;
	TArray<FVector3f> AxisX;
	TArray<FVector3f> AxisY;
	TArray<FVector3f> AxisZ;
	PositionPixels.Init(FFloat16Color(FLinearColor::Black), TexturePixelCount);
	QuatPixels.Init(FFloat16Color(FLinearColor(0.0f, 0.0f, 0.0f, 1.0f)), TexturePixelCount);
	ScaleAPixels.Init(FFloat16Color(FLinearColor::Black), TexturePixelCount);
	SH0Pixels.Init(FFloat16Color(FLinearColor::Black), TexturePixelCount);
	Positions.Reserve(PointCount);
	Colors.Reserve(PointCount);
	AxisX.Reserve(PointCount);
	AxisY.Reserve(PointCount);
	AxisZ.Reserve(PointCount);

	for (int32 PointIndex = 0; PointIndex < PointCount; ++PointIndex)
	{
		const float* PositionData = GaussianCloud.positions.data() + (PointIndex * 3);
		const float* ColorData = GaussianCloud.colors.data() + (PointIndex * 3);
		const float* ScaleData = GaussianCloud.scales.data() + (PointIndex * 3);
		const float* RotationData = GaussianCloud.rotations.data() + (PointIndex * 4);
		const float AlphaData = GaussianCloud.alphas[PointIndex];

		const FVector3f Position = ConvertSpzPositionToUnreal(PositionData);
		const FQuat4f Rotation = ConvertSpzQuaternionToUnreal(RotationData);
		const FVector3f Scale = ConvertSpzScaleToUnreal(ScaleData);
		const FLinearColor SH0 = ConvertSpzSH0ToUnreal(ColorData);
		const float Opacity = FMath::Clamp(Sigmoid(AlphaData), 0.0f, 1.0f);
		FVector3f PointAxisX;
		FVector3f PointAxisY;
		FVector3f PointAxisZ;

		PositionPixels[PointIndex] = FFloat16Color(FLinearColor(Position.X, Position.Y, Position.Z, 1.0f));
		QuatPixels[PointIndex] = FFloat16Color(FLinearColor(Rotation.X, Rotation.Y, Rotation.Z, Rotation.W));
		ScaleAPixels[PointIndex] = FFloat16Color(FLinearColor(Scale.X, Scale.Y, Scale.Z, Opacity));
		SH0Pixels[PointIndex] = FFloat16Color(FLinearColor(SH0.R, SH0.G, SH0.B, 1.0f));
		ConvertSpzAxesToUnreal(RotationData, ScaleData, PointAxisX, PointAxisY, PointAxisZ);
		Positions.Add(Position);
		Colors.Add(FLinearColor(SH0.R, SH0.G, SH0.B, Opacity));
		AxisX.Add(PointAxisX);
		AxisY.Add(PointAxisY);
		AxisZ.Add(PointAxisZ);
	}

	USpzGaussianActorRebuildAsset* ImportedAsset = NewObject<USpzGaussianActorRebuildAsset>(InParent, InClass, InName, Flags | RF_Transactional);
	ImportedAsset->SetRenderData(PointCount, MoveTemp(Positions), MoveTemp(Colors), MoveTemp(AxisX), MoveTemp(AxisY), MoveTemp(AxisZ));
	ImportedAsset->TextureWidth = TextureWidth;
	ImportedAsset->TextureHeight = TextureHeight;
	ImportedAsset->GeneratedAssetRoot = BuildGeneratedAssetRoot(InParent->GetOutermost()->GetName(), InName.ToString());
#if WITH_EDITOR
	ImportedAsset->UpdateImportData(Filename);
#endif

	const FString BaseAssetName = InName.ToString();
	ImportedAsset->TexPosition = CreateOrUpdatePackedTexture(
		ImportedAsset->GeneratedAssetRoot,
		FString::Printf(TEXT("T_GAR_Pos_%s"), *BaseAssetName),
		TextureWidth,
		TextureHeight,
		PositionPixels);
	ImportedAsset->TexQuat4 = CreateOrUpdatePackedTexture(
		ImportedAsset->GeneratedAssetRoot,
		FString::Printf(TEXT("T_GAR_Quat_%s"), *BaseAssetName),
		TextureWidth,
		TextureHeight,
		QuatPixels);
	ImportedAsset->TexScaleA = CreateOrUpdatePackedTexture(
		ImportedAsset->GeneratedAssetRoot,
		FString::Printf(TEXT("T_GAR_ScaleA_%s"), *BaseAssetName),
		TextureWidth,
		TextureHeight,
		ScaleAPixels);
	ImportedAsset->TexSH0 = CreateOrUpdatePackedTexture(
		ImportedAsset->GeneratedAssetRoot,
		FString::Printf(TEXT("T_GAR_SH0_%s"), *BaseAssetName),
		TextureWidth,
		TextureHeight,
		SH0Pixels);

	if (ImportedAsset->TexPosition == nullptr
		|| ImportedAsset->TexQuat4 == nullptr
		|| ImportedAsset->TexScaleA == nullptr
		|| ImportedAsset->TexSH0 == nullptr)
	{
		if (Warn != nullptr)
		{
			Warn->Logf(TEXT("SPZ Gaussian actor rebuild texture generation failed for %s"), *Filename);
		}
		return nullptr;
	}

	FString ErrorMessage;
	if (!SpzGaussianActorRebuildEditorUtilities::GenerateSupportAssets(*ImportedAsset, ErrorMessage))
	{
		if (Warn != nullptr)
		{
			Warn->Logf(TEXT("%s"), *ErrorMessage);
		}
		return nullptr;
	}

	ImportedAsset->MarkPackageDirty();
	return ImportedAsset;
}
