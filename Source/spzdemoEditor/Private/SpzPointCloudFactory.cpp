// Copyright Epic Games, Inc. All Rights Reserved.

#include "SpzPointCloudFactory.h"

#include "Logging/LogMacros.h"
#include "Misc/FeedbackContext.h"
#include "Misc/Paths.h"
#include "SpzEditorUtilities.h"
#include "SpzPointCloudAsset.h"

#include "load-spz.h"

namespace
{
	constexpr float SpzUnitsToCentimeters = 100.0f;
	constexpr float GaussianToSpriteScale = 6.0f;

	float Sigmoid(float Value)
	{
		return 1.0f / (1.0f + FMath::Exp(-Value));
	}

	float DecodeColorChannel(float Value)
	{
		return FMath::Clamp(0.5f + (0.282095f * Value), 0.0f, 1.0f);
	}

	FVector3f ConvertSpzPositionToUnreal(const float* PositionData)
	{
		const float X = -PositionData[2] * SpzUnitsToCentimeters;
		const float Y = PositionData[0] * SpzUnitsToCentimeters;
		const float Z = PositionData[1] * SpzUnitsToCentimeters;
		return FVector3f(X, Y, Z);
	}

	FLinearColor ConvertSpzColorToUnreal(const float* ColorData, float AlphaData)
	{
		return FLinearColor(
			DecodeColorChannel(ColorData[0]),
			DecodeColorChannel(ColorData[1]),
			DecodeColorChannel(ColorData[2]),
			FMath::Clamp(Sigmoid(AlphaData), 0.0f, 1.0f));
	}

	float ConvertSpzScaleToSpriteSize(const float* ScaleData, float AlphaData)
	{
		const float AverageLogScale = (ScaleData[0] + ScaleData[1] + ScaleData[2]) / 3.0f;
		const float GaussianSizeCm = FMath::Exp(AverageLogScale) * SpzUnitsToCentimeters * GaussianToSpriteScale;
		const float AlphaWeight = FMath::Lerp(0.85f, 1.15f, FMath::Clamp(Sigmoid(AlphaData), 0.0f, 1.0f));
		return FMath::Max(1.0f, GaussianSizeCm * AlphaWeight);
	}
}

USpzPointCloudFactory::USpzPointCloudFactory()
{
	SupportedClass = USpzPointCloudAsset::StaticClass();
	bCreateNew = false;
	bEditorImport = true;
	bText = false;
	Formats.Add(TEXT("spz;SPZ Gaussian point cloud"));
}

bool USpzPointCloudFactory::FactoryCanImport(const FString& Filename)
{
	return FPaths::GetExtension(Filename).Equals(TEXT("spz"), ESearchCase::IgnoreCase);
}

UObject* USpzPointCloudFactory::FactoryCreateFile(
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
			Warn->Logf(TEXT("SPZ import failed for %s"), *Filename);
		}
		return nullptr;
	}

	TArray<FVector3f> Positions;
	TArray<FLinearColor> Colors;
	TArray<float> Sizes;
	Positions.Reserve(GaussianCloud.numPoints);
	Colors.Reserve(GaussianCloud.numPoints);
	Sizes.Reserve(GaussianCloud.numPoints);

	for (int32 PointIndex = 0; PointIndex < GaussianCloud.numPoints; ++PointIndex)
	{
		const float* PositionData = GaussianCloud.positions.data() + (PointIndex * 3);
		const float* ColorData = GaussianCloud.colors.data() + (PointIndex * 3);
		const float* ScaleData = GaussianCloud.scales.data() + (PointIndex * 3);
		const float AlphaData = GaussianCloud.alphas[PointIndex];

		Positions.Add(ConvertSpzPositionToUnreal(PositionData));
		Colors.Add(ConvertSpzColorToUnreal(ColorData, AlphaData));
		Sizes.Add(ConvertSpzScaleToSpriteSize(ScaleData, AlphaData));
	}

	USpzPointCloudAsset* PointCloudAsset = NewObject<USpzPointCloudAsset>(InParent, InClass, InName, Flags | RF_Transactional);
	PointCloudAsset->SetRenderData(GaussianCloud.numPoints, MoveTemp(Positions), MoveTemp(Colors), MoveTemp(Sizes));
#if WITH_EDITOR
	PointCloudAsset->UpdateImportData(Filename);
#endif
	PointCloudAsset->MarkPackageDirty();

	FSpzEditorUtilities::GeneratePointCloudSupportAssets(*PointCloudAsset);
	return PointCloudAsset;
}
