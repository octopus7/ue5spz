// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "SpzPointCloudAsset.generated.h"

class UAssetImportData;

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
	bool HasRenderableData() const;

	void SetRenderData(int32 InSourcePointCount, TArray<FVector3f>&& InPositions, TArray<FLinearColor>&& InColors, TArray<float>&& InSizes);
	void BuildRenderArrays(int32 MaxPoints, float SizeMultiplier, TArray<FVector>& OutPositions, TArray<FLinearColor>& OutColors, TArray<FVector2D>& OutSpriteSizes) const;

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
	TArray<float> Sizes;

#if WITH_EDITORONLY_DATA
	UPROPERTY(VisibleAnywhere, Instanced, Category = "Import Settings")
	TObjectPtr<UAssetImportData> AssetImportData;
#endif

private:
	void UpdateBounds();
};
