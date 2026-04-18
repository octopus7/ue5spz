#include "SpzGaussianActorRebuildAsset.h"

#if WITH_EDITOR
#include "EditorFramework/AssetImportData.h"
#endif

bool USpzGaussianActorRebuildAsset::HasRenderableData() const
{
	return Positions.Num() > 0
		&& Positions.Num() == Colors.Num()
		&& Positions.Num() == AxisX.Num()
		&& Positions.Num() == AxisY.Num()
		&& Positions.Num() == AxisZ.Num();
}

void USpzGaussianActorRebuildAsset::AppendPointToRenderPoints(
	int32 Index,
	float ScaleMultiplier,
	TArray<FSpzGaussianActorRebuildRenderPoint>& OutPoints) const
{
	FSpzGaussianActorRebuildRenderPoint& RenderPoint = OutPoints.AddDefaulted_GetRef();
	RenderPoint.Position = Positions[Index];
	RenderPoint.Color = Colors[Index];
	RenderPoint.AxisX = AxisX[Index] * ScaleMultiplier;
	RenderPoint.AxisY = AxisY[Index] * ScaleMultiplier;
	RenderPoint.AxisZ = AxisZ[Index] * ScaleMultiplier;
}

void USpzGaussianActorRebuildAsset::SetRenderData(
	int32 InPointCount,
	TArray<FVector3f>&& InPositions,
	TArray<FLinearColor>&& InColors,
	TArray<FVector3f>&& InAxisX,
	TArray<FVector3f>&& InAxisY,
	TArray<FVector3f>&& InAxisZ)
{
	PointCount = InPointCount;
	Positions = MoveTemp(InPositions);
	Colors = MoveTemp(InColors);
	AxisX = MoveTemp(InAxisX);
	AxisY = MoveTemp(InAxisY);
	AxisZ = MoveTemp(InAxisZ);
	++RenderDataVersion;
	UpdateBounds();
}

void USpzGaussianActorRebuildAsset::BuildRenderPoints(
	int32 MaxPoints,
	float ScaleMultiplier,
	TArray<FSpzGaussianActorRebuildRenderPoint>& OutPoints) const
{
	OutPoints.Reset();

	if (!HasRenderableData() || MaxPoints <= 0)
	{
		return;
	}

	const int32 TotalPoints = Positions.Num();
	const int32 TargetPointCount = FMath::Min(MaxPoints, TotalPoints);
	const float SafeScaleMultiplier = FMath::Max(ScaleMultiplier, KINDA_SMALL_NUMBER);

	OutPoints.Reserve(TargetPointCount);

	if (TargetPointCount == TotalPoints)
	{
		for (int32 Index = 0; Index < TotalPoints; ++Index)
		{
			AppendPointToRenderPoints(Index, SafeScaleMultiplier, OutPoints);
		}
		return;
	}

	const double Step = static_cast<double>(TotalPoints) / static_cast<double>(TargetPointCount);
	double Cursor = 0.0;

	for (int32 OutputIndex = 0; OutputIndex < TargetPointCount; ++OutputIndex)
	{
		const int32 SourceIndex = FMath::Clamp(static_cast<int32>(Cursor), 0, TotalPoints - 1);
		AppendPointToRenderPoints(SourceIndex, SafeScaleMultiplier, OutPoints);
		Cursor += Step;
	}
}

#if WITH_EDITOR
void USpzGaussianActorRebuildAsset::UpdateImportData(const FString& SourceFilename)
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

void USpzGaussianActorRebuildAsset::UpdateBounds()
{
	if (Positions.Num() == 0)
	{
		BoundsOrigin = FVector::ZeroVector;
		BoundsExtent = FVector::ZeroVector;
		return;
	}

	FBox Bounds(EForceInit::ForceInit);
	for (int32 Index = 0; Index < Positions.Num(); ++Index)
	{
		const FVector Position(Positions[Index]);
		const FVector AxisExtent(
			FMath::Abs(AxisX[Index].X) + FMath::Abs(AxisY[Index].X) + FMath::Abs(AxisZ[Index].X),
			FMath::Abs(AxisX[Index].Y) + FMath::Abs(AxisY[Index].Y) + FMath::Abs(AxisZ[Index].Y),
			FMath::Abs(AxisX[Index].Z) + FMath::Abs(AxisY[Index].Z) + FMath::Abs(AxisZ[Index].Z));

		Bounds += Position - AxisExtent;
		Bounds += Position + AxisExtent;
	}

	BoundsOrigin = Bounds.GetCenter();
	BoundsExtent = Bounds.GetExtent();
}
