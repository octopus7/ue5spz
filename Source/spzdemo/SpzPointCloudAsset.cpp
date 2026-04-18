// Copyright Epic Games, Inc. All Rights Reserved.

#include "SpzPointCloudAsset.h"

#include "Algo/Accumulate.h"

#if WITH_EDITOR
#include "EditorFramework/AssetImportData.h"
#endif

bool USpzPointCloudAsset::HasRenderableData() const
{
	return Positions.Num() > 0
		&& Positions.Num() == Colors.Num()
		&& Positions.Num() == AxisX.Num()
		&& Positions.Num() == AxisY.Num()
		&& Positions.Num() == AxisZ.Num();
}

void USpzPointCloudAsset::AppendPointToRenderPoints(int32 Index, float ScaleMultiplier, TArray<FSpzSplatRenderPoint>& OutPoints) const
{
	FSpzSplatRenderPoint& RenderPoint = OutPoints.AddDefaulted_GetRef();
	RenderPoint.Position = Positions[Index];
	RenderPoint.Color = Colors[Index];
	RenderPoint.AxisX = AxisX[Index] * ScaleMultiplier;
	RenderPoint.AxisY = AxisY[Index] * ScaleMultiplier;
	RenderPoint.AxisZ = AxisZ[Index] * ScaleMultiplier;
}

void USpzPointCloudAsset::SetRenderData(
	int32 InSourcePointCount,
	TArray<FVector3f>&& InPositions,
	TArray<FLinearColor>&& InColors,
	TArray<FVector3f>&& InAxisX,
	TArray<FVector3f>&& InAxisY,
	TArray<FVector3f>&& InAxisZ)
{
	SourcePointCount = InSourcePointCount;
	Positions = MoveTemp(InPositions);
	Colors = MoveTemp(InColors);
	AxisX = MoveTemp(InAxisX);
	AxisY = MoveTemp(InAxisY);
	AxisZ = MoveTemp(InAxisZ);
	++RenderDataVersion;

	UpdateBounds();

	if (AxisX.Num() > 0)
	{
		double SizeSum = 0.0;
		for (int32 Index = 0; Index < AxisX.Num(); ++Index)
		{
			const float MaxAxis = FMath::Max3(AxisX[Index].Length(), AxisY[Index].Length(), AxisZ[Index].Length());
			SizeSum += MaxAxis * 6.0;
		}

		SuggestedSpriteSize = static_cast<float>(SizeSum / static_cast<double>(AxisX.Num()));
	}
	else
	{
		SuggestedSpriteSize = 1.0f;
	}
}

void USpzPointCloudAsset::BuildRenderPoints(int32 MaxPoints, float ScaleMultiplier, TArray<FSpzSplatRenderPoint>& OutPoints) const
{
	OutPoints.Reset();

	if (!HasRenderableData() || MaxPoints <= 0)
	{
		return;
	}

	const int32 TotalPoints = Positions.Num();
	const int32 TargetPointCount = FMath::Min(MaxPoints, TotalPoints);
	const float SafeMultiplier = FMath::Max(ScaleMultiplier, KINDA_SMALL_NUMBER);

	OutPoints.Reserve(TargetPointCount);

	if (TargetPointCount == TotalPoints)
	{
		for (int32 Index = 0; Index < TotalPoints; ++Index)
		{
			AppendPointToRenderPoints(Index, SafeMultiplier, OutPoints);
		}
		return;
	}

	const double Step = static_cast<double>(TotalPoints) / static_cast<double>(TargetPointCount);
	double Cursor = 0.0;

	for (int32 OutputIndex = 0; OutputIndex < TargetPointCount; ++OutputIndex)
	{
		const int32 SourceIndex = FMath::Clamp(static_cast<int32>(Cursor), 0, TotalPoints - 1);
		AppendPointToRenderPoints(SourceIndex, SafeMultiplier, OutPoints);
		Cursor += Step;
	}
}

void USpzPointCloudAsset::BuildRenderPointsForView(
	int32 MaxPoints,
	float ScaleMultiplier,
	const FVector& ViewLocationLocal,
	const FVector& ViewForwardLocal,
	float HorizontalFovDegrees,
	float FovScale,
	TArray<FSpzSplatRenderPoint>& OutPoints) const
{
	OutPoints.Reset();

	if (!HasRenderableData() || MaxPoints <= 0)
	{
		return;
	}

	const int32 TotalPoints = Positions.Num();
	const int32 TargetPointCount = FMath::Min(MaxPoints, TotalPoints);
	const float SafeMultiplier = FMath::Max(ScaleMultiplier, KINDA_SMALL_NUMBER);
	FVector SafeViewForward = ViewForwardLocal.GetSafeNormal();
	if (SafeViewForward.IsNearlyZero())
	{
		SafeViewForward = FVector::ForwardVector;
	}
	const float EffectiveHalfFovDegrees = FMath::Clamp(HorizontalFovDegrees * 0.5f * FMath::Max(FovScale, 0.1f), 5.0f, 89.5f);
	const float CosThreshold = FMath::Cos(FMath::DegreesToRadians(EffectiveHalfFovDegrees));
	const float CosThresholdSq = CosThreshold * CosThreshold;

	TArray<int32> VisibleIndices;
	VisibleIndices.Reserve(FMath::Min(TotalPoints, TargetPointCount * 4));

	for (int32 Index = 0; Index < TotalPoints; ++Index)
	{
		const FVector ToPoint = FVector(Positions[Index]) - ViewLocationLocal;
		const float DistanceSq = ToPoint.SizeSquared();
		if (DistanceSq <= KINDA_SMALL_NUMBER)
		{
			VisibleIndices.Add(Index);
			continue;
		}

		const float ForwardDistance = FVector::DotProduct(ToPoint, SafeViewForward);
		if (ForwardDistance <= 0.0f)
		{
			continue;
		}

		if ((ForwardDistance * ForwardDistance) >= (DistanceSq * CosThresholdSq))
		{
			VisibleIndices.Add(Index);
		}
	}

	if (VisibleIndices.Num() == 0)
	{
		return;
	}

	const int32 OutputPointCount = FMath::Min(TargetPointCount, VisibleIndices.Num());
	OutPoints.Reserve(OutputPointCount);

	if (OutputPointCount == VisibleIndices.Num())
	{
		for (int32 SourceIndex : VisibleIndices)
		{
			AppendPointToRenderPoints(SourceIndex, SafeMultiplier, OutPoints);
		}
		return;
	}

	const double Step = static_cast<double>(VisibleIndices.Num()) / static_cast<double>(OutputPointCount);
	double Cursor = 0.0;

	for (int32 OutputIndex = 0; OutputIndex < OutputPointCount; ++OutputIndex)
	{
		const int32 VisibleIndex = FMath::Clamp(static_cast<int32>(Cursor), 0, VisibleIndices.Num() - 1);
		AppendPointToRenderPoints(VisibleIndices[VisibleIndex], SafeMultiplier, OutPoints);
		Cursor += Step;
	}
}

#if WITH_EDITOR
void USpzPointCloudAsset::UpdateImportData(const FString& SourceFilename)
{
	if (AssetImportData == nullptr)
	{
		AssetImportData = NewObject<UAssetImportData>(this, TEXT("AssetImportData"));
	}

	if (AssetImportData != nullptr)
	{
		AssetImportData->Update(SourceFilename);
	}
}
#endif

void USpzPointCloudAsset::UpdateBounds()
{
	if (Positions.Num() == 0)
	{
		BoundsOrigin = FVector::ZeroVector;
		BoundsExtent = FVector::ZeroVector;
		return;
	}

	FBox Box(EForceInit::ForceInit);
	for (int32 Index = 0; Index < Positions.Num(); ++Index)
	{
		const FVector Position = FVector(Positions[Index]);
		const FVector AxisExtent(
			FMath::Abs(AxisX[Index].X) + FMath::Abs(AxisY[Index].X) + FMath::Abs(AxisZ[Index].X),
			FMath::Abs(AxisX[Index].Y) + FMath::Abs(AxisY[Index].Y) + FMath::Abs(AxisZ[Index].Y),
			FMath::Abs(AxisX[Index].Z) + FMath::Abs(AxisY[Index].Z) + FMath::Abs(AxisZ[Index].Z));

		Box += Position - AxisExtent;
		Box += Position + AxisExtent;
	}

	BoundsOrigin = Box.GetCenter();
	BoundsExtent = Box.GetExtent();
}
