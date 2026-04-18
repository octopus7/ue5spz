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
		&& Positions.Num() == Sizes.Num();
}

void USpzPointCloudAsset::SetRenderData(int32 InSourcePointCount, TArray<FVector3f>&& InPositions, TArray<FLinearColor>&& InColors, TArray<float>&& InSizes)
{
	SourcePointCount = InSourcePointCount;
	Positions = MoveTemp(InPositions);
	Colors = MoveTemp(InColors);
	Sizes = MoveTemp(InSizes);

	UpdateBounds();

	if (Sizes.Num() > 0)
	{
		const double SizeSum = Algo::Accumulate(Sizes, 0.0);
		SuggestedSpriteSize = static_cast<float>(SizeSum / static_cast<double>(Sizes.Num()));
	}
	else
	{
		SuggestedSpriteSize = 1.0f;
	}
}

void USpzPointCloudAsset::BuildRenderArrays(int32 MaxPoints, float SizeMultiplier, TArray<FVector>& OutPositions, TArray<FLinearColor>& OutColors, TArray<FVector2D>& OutSpriteSizes) const
{
	OutPositions.Reset();
	OutColors.Reset();
	OutSpriteSizes.Reset();

	if (!HasRenderableData() || MaxPoints <= 0)
	{
		return;
	}

	const int32 TotalPoints = Positions.Num();
	const int32 TargetPointCount = FMath::Min(MaxPoints, TotalPoints);
	const float SafeMultiplier = FMath::Max(SizeMultiplier, KINDA_SMALL_NUMBER);

	OutPositions.Reserve(TargetPointCount);
	OutColors.Reserve(TargetPointCount);
	OutSpriteSizes.Reserve(TargetPointCount);

	const auto AppendPoint = [this, SafeMultiplier, &OutPositions, &OutColors, &OutSpriteSizes](int32 Index)
	{
		OutPositions.Add(FVector(Positions[Index]));
		OutColors.Add(Colors[Index]);

		const float SpriteSize = FMath::Max(1.0f, Sizes[Index] * SafeMultiplier);
		OutSpriteSizes.Add(FVector2D(SpriteSize, SpriteSize));
	};

	if (TargetPointCount == TotalPoints)
	{
		for (int32 Index = 0; Index < TotalPoints; ++Index)
		{
			AppendPoint(Index);
		}
		return;
	}

	const double Step = static_cast<double>(TotalPoints) / static_cast<double>(TargetPointCount);
	double Cursor = 0.0;

	for (int32 OutputIndex = 0; OutputIndex < TargetPointCount; ++OutputIndex)
	{
		const int32 SourceIndex = FMath::Clamp(static_cast<int32>(Cursor), 0, TotalPoints - 1);
		AppendPoint(SourceIndex);
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
	for (const FVector3f& Position : Positions)
	{
		Box += FVector(Position);
	}

	BoundsOrigin = Box.GetCenter();
	BoundsExtent = Box.GetExtent();
}
