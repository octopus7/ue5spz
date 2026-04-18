#pragma once

#include "CoreMinimal.h"
#include "Components/MeshComponent.h"
#include "SpzGaussianActorRebuildAsset.h"
#include "SpzGaussianActorRebuildSplatComponent.generated.h"

UCLASS(ClassGroup = (Rendering), meta = (BlueprintSpawnableComponent))
class SPZGAUSSIANACTORREBUILD_API USpzGaussianActorRebuildSplatComponent : public UMeshComponent
{
	GENERATED_BODY()

public:
	USpzGaussianActorRebuildSplatComponent();

	void SetRenderPoints(
		TArray<FSpzGaussianActorRebuildRenderPoint>&& InRenderPoints,
		const FVector& InLocalBoundsOrigin,
		const FVector& InLocalBoundsExtent);
	void ClearRenderPoints();

	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	virtual int32 GetNumMaterials() const override;

	const TArray<FSpzGaussianActorRebuildRenderPoint>& GetRenderPoints() const
	{
		return RenderPoints;
	}

private:
	TArray<FSpzGaussianActorRebuildRenderPoint> RenderPoints;
	FVector LocalBoundsOrigin = FVector::ZeroVector;
	FVector LocalBoundsExtent = FVector::ZeroVector;
};
