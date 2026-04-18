// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/MeshComponent.h"
#include "SpzPointCloudAsset.h"
#include "SpzGaussianSplatComponent.generated.h"

UCLASS(ClassGroup = (Rendering), meta = (BlueprintSpawnableComponent))
class SPZDEMO_API USpzGaussianSplatComponent : public UMeshComponent
{
	GENERATED_BODY()

public:
	USpzGaussianSplatComponent();

	void SetRenderPoints(TArray<FSpzSplatRenderPoint>&& InRenderPoints, const FVector& InLocalBoundsOrigin, const FVector& InLocalBoundsExtent);
	void ClearRenderPoints();

	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	virtual int32 GetNumMaterials() const override;

	const TArray<FSpzSplatRenderPoint>& GetRenderPoints() const
	{
		return RenderPoints;
	}

	FVector GetLocalBoundsOrigin() const
	{
		return LocalBoundsOrigin;
	}

	FVector GetLocalBoundsExtent() const
	{
		return LocalBoundsExtent;
	}

private:
	TArray<FSpzSplatRenderPoint> RenderPoints;
	FVector LocalBoundsOrigin = FVector::ZeroVector;
	FVector LocalBoundsExtent = FVector::ZeroVector;
};
