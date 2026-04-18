// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "SpzPointCloudAsset.generated.h"

class UAssetImportData;

struct FSpzSplatRenderPoint
{
	FVector3f Position = FVector3f::ZeroVector;
	FLinearColor Color = FLinearColor::White;
	FVector3f AxisX = FVector3f::ZeroVector;
	FVector3f AxisY = FVector3f::ZeroVector;
	FVector3f AxisZ = FVector3f::ZeroVector;
};

UCLASS(BlueprintType)
class SPZDEMO_API USpzPointCloudAsset : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "SPZ")
	int32 GetSourcePointCount() const
	{
		return SourcePointCount;
	}

	UFUNCTION(BlueprintPure, Category = "SPZ")
	int32 GetStoredPointCount() const
	{
		return Positions.Num();
	}

	UFUNCTION(BlueprintPure, Category = "SPZ")
	int32 GetRenderDataVersion() const
	{
		return RenderDataVersion;
	}

	UFUNCTION(BlueprintPure, Category = "SPZ")
	bool HasRenderableData() const;

	void SetRenderData(
		int32 InSourcePointCount,
		TArray<FVector3f>&& InPositions,
		TArray<FLinearColor>&& InColors,
		TArray<FVector3f>&& InAxisX,
		TArray<FVector3f>&& InAxisY,
		TArray<FVector3f>&& InAxisZ);
	void BuildRenderPoints(int32 MaxPoints, float ScaleMultiplier, TArray<FSpzSplatRenderPoint>& OutPoints) const;
	void BuildRenderPointsForView(
		int32 MaxPoints,
		float ScaleMultiplier,
		const FVector& ViewLocationLocal,
		const FVector& ViewForwardLocal,
		float HorizontalFovDegrees,
		float FovScale,
		TArray<FSpzSplatRenderPoint>& OutPoints) const;

#if WITH_EDITOR
	void UpdateImportData(const FString& SourceFilename);
#endif

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SPZ")
	int32 SourcePointCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SPZ")
	FVector BoundsOrigin = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SPZ")
	FVector BoundsExtent = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SPZ")
	float SuggestedSpriteSize = 1.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SPZ|Data", AdvancedDisplay)
	TArray<FVector3f> Positions;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SPZ|Data", AdvancedDisplay)
	TArray<FLinearColor> Colors;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SPZ|Data", AdvancedDisplay)
	TArray<FVector3f> AxisX;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SPZ|Data", AdvancedDisplay)
	TArray<FVector3f> AxisY;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SPZ|Data", AdvancedDisplay)
	TArray<FVector3f> AxisZ;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SPZ|Data", AdvancedDisplay)
	int32 RenderDataVersion = 0;

#if WITH_EDITORONLY_DATA
	UPROPERTY(VisibleAnywhere, Instanced, Category = "Import Settings")
	TObjectPtr<UAssetImportData> AssetImportData;
#endif

private:
	void AppendPointToRenderPoints(int32 Index, float ScaleMultiplier, TArray<FSpzSplatRenderPoint>& OutPoints) const;
	void UpdateBounds();
};
