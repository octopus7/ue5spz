#pragma once

#include "CoreMinimal.h"
#include "Components/PrimitiveComponent.h"
#include "SpzSplatData.h"
#include "SpzComponent.generated.h"

class USpzAsset;

UCLASS(ClassGroup = Rendering, BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class SPZRUNTIME_API USpzComponent : public UPrimitiveComponent
{
	GENERATED_BODY()

public:
	USpzComponent(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SPZ")
	TObjectPtr<USpzAsset> SpzAsset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SPZ|Sorting")
	ESpzSortMode SortMode = ESpzSortMode::CameraDeltaThreshold;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SPZ|Sorting", meta = (ClampMin = "0.0"))
	float SortInterval = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SPZ|Sorting", meta = (ClampMin = "0.0"))
	float MinCameraMoveDistance = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SPZ|Sorting", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float MinCameraAngleDegrees = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SPZ|Preview", meta = (ClampMin = "1.0"))
	float PreviewPointSize = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SPZ|Preview", meta = (ClampMin = "0"))
	int32 MaxPreviewSplats = 250000;

	UFUNCTION(BlueprintCallable, Category = "SPZ")
	void SetSpzAsset(USpzAsset* InAsset);

	UFUNCTION(BlueprintCallable, Category = "SPZ|Sorting")
	void RequestSortNow();

	const TArray<int32>& GetSortedIndices() const { return SortedIndices; }

	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	virtual int32 GetNumMaterials() const override { return 0; }
	virtual void OnRegister() override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	UPROPERTY(Transient)
	TArray<int32> SortedIndices;

	float TimeSinceLastSort = 0.0f;
	bool bHasLastCamera = false;
	FVector LastCameraLocation = FVector::ZeroVector;
	FVector LastCameraForward = FVector::ForwardVector;

	bool ResolveSortCamera(FVector& OutLocation, FVector& OutForward) const;
	void RebuildSortOrder(const FVector& CameraLocation, const FVector& CameraForward);
	bool ShouldSortForCamera(const FVector& CameraLocation, const FVector& CameraForward) const;
};
