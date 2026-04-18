// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SpzNiagaraParameters.h"
#include "SpzNiagaraPointCloudActor.generated.h"

class UNiagaraComponent;
class UNiagaraSystem;
class USceneComponent;
class USpzPointCloudAsset;

UCLASS(BlueprintType, Blueprintable)
class SPZDEMO_API ASpzNiagaraPointCloudActor : public AActor
{
	GENERATED_BODY()

public:
	ASpzNiagaraPointCloudActor();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "SPZ")
	void RefreshPointCloud();

	UFUNCTION(BlueprintCallable, Category = "SPZ")
	void SetPointCloudAsset(USpzPointCloudAsset* NewPointCloudAsset);

	UFUNCTION(BlueprintPure, Category = "SPZ")
	int32 GetActiveParticleCount() const
	{
		return LastAppliedParticleCount;
	}

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UNiagaraComponent> NiagaraComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SPZ")
	TObjectPtr<USpzPointCloudAsset> PointCloudAsset;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SPZ")
	TObjectPtr<UNiagaraSystem> NiagaraSystemAsset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SPZ", meta = (ClampMin = "1", UIMin = "1"))
	int32 MaxRenderPoints = SpzNiagaraParameters::DefaultMaxRenderPoints;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SPZ", meta = (ClampMin = "0.001", UIMin = "0.001"))
	float SpriteSizeMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SPZ", meta = (ClampMin = "0.1", UIMin = "0.1"))
	float ParticleLifetimeSeconds = SpzNiagaraParameters::DefaultParticleLifetimeSeconds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SPZ")
	bool bRefreshInConstructionScript = true;

protected:
	void ApplyPointCloudToNiagara(bool bForceResetSystem);
	bool CanApplyPointCloud() const;

	UPROPERTY(Transient)
	int32 LastAppliedParticleCount = 0;
};
