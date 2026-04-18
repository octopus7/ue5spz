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
		const float X = -DirectionData.Z;
		const float Y = DirectionData.X;
		const float Z = DirectionData.Y;
		return FVector3f(X, Y, Z);
	}

	FVector3f ConvertSpzPositionToUnreal(const float* PositionData)
	{
		return ConvertSpzDirectionToUnreal(FVector3f(PositionData[0], PositionData[1], PositionData[2]) * SpzUnitsToCentimeters);
	}

	FLinearColor ConvertSpzColorToUnreal(const float* ColorData, float AlphaData)
	{
		return FLinearColor(
			DecodeColorChannel(ColorData[0]),
			DecodeColorChannel(ColorData[1]),
			DecodeColorChannel(ColorData[2]),
			FMath::Clamp(Sigmoid(AlphaData), 0.0f, 1.0f));
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
	TArray<FVector3f> AxisX;
	TArray<FVector3f> AxisY;
	TArray<FVector3f> AxisZ;
	Positions.Reserve(GaussianCloud.numPoints);
	Colors.Reserve(GaussianCloud.numPoints);
	AxisX.Reserve(GaussianCloud.numPoints);
	AxisY.Reserve(GaussianCloud.numPoints);
	AxisZ.Reserve(GaussianCloud.numPoints);

	for (int32 PointIndex = 0; PointIndex < GaussianCloud.numPoints; ++PointIndex)
	{
		const float* PositionData = GaussianCloud.positions.data() + (PointIndex * 3);
		const float* ColorData = GaussianCloud.colors.data() + (PointIndex * 3);
		const float* ScaleData = GaussianCloud.scales.data() + (PointIndex * 3);
		const float* RotationData = GaussianCloud.rotations.data() + (PointIndex * 4);
		const float AlphaData = GaussianCloud.alphas[PointIndex];
		FVector3f PointAxisX;
		FVector3f PointAxisY;
		FVector3f PointAxisZ;

		Positions.Add(ConvertSpzPositionToUnreal(PositionData));
		Colors.Add(ConvertSpzColorToUnreal(ColorData, AlphaData));
		ConvertSpzAxesToUnreal(RotationData, ScaleData, PointAxisX, PointAxisY, PointAxisZ);
		AxisX.Add(PointAxisX);
		AxisY.Add(PointAxisY);
		AxisZ.Add(PointAxisZ);
	}

	USpzPointCloudAsset* PointCloudAsset = NewObject<USpzPointCloudAsset>(InParent, InClass, InName, Flags | RF_Transactional);
	PointCloudAsset->SetRenderData(GaussianCloud.numPoints, MoveTemp(Positions), MoveTemp(Colors), MoveTemp(AxisX), MoveTemp(AxisY), MoveTemp(AxisZ));
#if WITH_EDITOR
	PointCloudAsset->UpdateImportData(Filename);
#endif
	PointCloudAsset->MarkPackageDirty();

	FSpzEditorUtilities::GeneratePointCloudSupportAssets(*PointCloudAsset);
	return PointCloudAsset;
}
