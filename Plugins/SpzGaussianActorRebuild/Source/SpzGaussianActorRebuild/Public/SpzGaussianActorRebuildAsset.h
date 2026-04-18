#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "SpzGaussianActorRebuildAsset.generated.h"

class UAssetImportData;
class UBlueprint;
class UMaterialInterface;
class UTexture2D;

struct FSpzGaussianActorRebuildRenderPoint
{
	FVector3f Position = FVector3f::ZeroVector;
	FLinearColor Color = FLinearColor::White;
	FVector3f AxisX = FVector3f::ZeroVector;
	FVector3f AxisY = FVector3f::ZeroVector;
	FVector3f AxisZ = FVector3f::ZeroVector;
};

UCLASS(BlueprintType)
class SPZGAUSSIANACTORREBUILD_API USpzGaussianActorRebuildAsset : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Gaussian Actor")
	int32 GetStoredPointCount() const
	{
		return Positions.Num();
	}

	UFUNCTION(BlueprintPure, Category = "Gaussian Actor")
	int32 GetRenderDataVersion() const
	{
		return RenderDataVersion;
	}

	UFUNCTION(BlueprintPure, Category = "Gaussian Actor")
	bool HasRenderableData() const;

	void SetRenderData(
		int32 InPointCount,
		TArray<FVector3f>&& InPositions,
		TArray<FLinearColor>&& InColors,
		TArray<FVector3f>&& InAxisX,
		TArray<FVector3f>&& InAxisY,
		TArray<FVector3f>&& InAxisZ);
	void BuildRenderPoints(int32 MaxPoints, float ScaleMultiplier, TArray<FSpzGaussianActorRebuildRenderPoint>& OutPoints) const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gaussian Actor")
	int32 PointCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gaussian Actor")
	int32 TextureWidth = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gaussian Actor")
	int32 TextureHeight = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gaussian Actor")
	FVector BoundsOrigin = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gaussian Actor")
	FVector BoundsExtent = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gaussian Actor")
	TObjectPtr<UTexture2D> TexPosition;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gaussian Actor")
	TObjectPtr<UTexture2D> TexQuat4;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gaussian Actor")
	TObjectPtr<UTexture2D> TexScaleA;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gaussian Actor")
	TObjectPtr<UTexture2D> TexSH0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gaussian Actor|Generated")
	TObjectPtr<UMaterialInterface> UnlitMaterial;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gaussian Actor|Generated")
	TObjectPtr<UMaterialInterface> RelightMaterial;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gaussian Actor|Generated")
	TObjectPtr<UBlueprint> ActorBlueprint;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gaussian Actor|Generated")
	FString GeneratedAssetRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gaussian Actor|Data", AdvancedDisplay)
	TArray<FVector3f> Positions;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gaussian Actor|Data", AdvancedDisplay)
	TArray<FLinearColor> Colors;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gaussian Actor|Data", AdvancedDisplay)
	TArray<FVector3f> AxisX;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gaussian Actor|Data", AdvancedDisplay)
	TArray<FVector3f> AxisY;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gaussian Actor|Data", AdvancedDisplay)
	TArray<FVector3f> AxisZ;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gaussian Actor|Data", AdvancedDisplay)
	int32 RenderDataVersion = 0;

#if WITH_EDITORONLY_DATA
	UPROPERTY(VisibleAnywhere, Instanced, Category = "Import Settings")
	TObjectPtr<UAssetImportData> AssetImportData;
#endif

#if WITH_EDITOR
	void UpdateImportData(const FString& SourceFilename);
#endif

private:
	void AppendPointToRenderPoints(int32 Index, float ScaleMultiplier, TArray<FSpzGaussianActorRebuildRenderPoint>& OutPoints) const;
	void UpdateBounds();
};
