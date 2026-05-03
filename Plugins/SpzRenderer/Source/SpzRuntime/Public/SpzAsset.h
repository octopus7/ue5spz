#pragma once

#include "CoreMinimal.h"
#include "SpzSplatData.h"
#include "UObject/Object.h"
#include "SpzAsset.generated.h"

class UAssetImportData;

UCLASS(BlueprintType)
class SPZRUNTIME_API USpzAsset : public UObject
{
	GENERATED_BODY()

public:
	USpzAsset();

#if WITH_EDITORONLY_DATA
	UPROPERTY(VisibleAnywhere, Instanced, Category = "Import")
	TObjectPtr<UAssetImportData> AssetImportData;
#endif

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SPZ")
	int32 NumSplats = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SPZ")
	int32 ShDegree = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SPZ")
	int32 ShCoefficientsPerSplat = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SPZ")
	bool bAntialiased = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SPZ")
	FBoxSphereBounds LocalBounds = FBoxSphereBounds(EForceInit::ForceInit);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SPZ")
	FString SourceFormatVersion;

	UPROPERTY(VisibleAnywhere, Category = "SPZ")
	TArray<FVector3f> Positions;

	UPROPERTY(VisibleAnywhere, Category = "SPZ")
	TArray<FVector4f> Rotations;

	UPROPERTY(VisibleAnywhere, Category = "SPZ")
	TArray<FVector3f> Scales;

	UPROPERTY(VisibleAnywhere, Category = "SPZ")
	TArray<FColor> Colors;

	UPROPERTY(VisibleAnywhere, Category = "SPZ")
	TArray<float> SphericalHarmonics;

	UFUNCTION(BlueprintPure, Category = "SPZ")
	int32 GetSplatCount() const { return NumSplats; }

	void InitializeFromArrays(
		TArray<FVector3f>&& InPositions,
		TArray<FVector4f>&& InRotations,
		TArray<FVector3f>&& InScales,
		TArray<FColor>&& InColors,
		TArray<float>&& InSphericalHarmonics,
		int32 InShDegree,
		bool bInAntialiased,
		const FString& InSourceFormatVersion);

	void RebuildBounds();

	virtual void PostLoad() override;
};
