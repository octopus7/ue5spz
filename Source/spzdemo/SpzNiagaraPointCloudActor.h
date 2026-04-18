// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SpzNiagaraParameters.h"
#include "SpzPointCloudAsset.h"
#include "SpzNiagaraPointCloudActor.generated.h"

class UNiagaraComponent;
class UNiagaraSystem;
class UMaterialInterface;
class USceneComponent;
class USpzGaussianSplatComponent;

UCLASS(BlueprintType, Blueprintable)
class SPZDEMO_API ASpzNiagaraPointCloudActor : public AActor
{
	GENERATED_BODY()

public:
	ASpzNiagaraPointCloudActor();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual bool ShouldTickIfViewportsOnly() const override;

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
	TObjectPtr<USceneComponent> RenderRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UNiagaraComponent> NiagaraComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USpzGaussianSplatComponent> SplatComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SPZ")
	TObjectPtr<USpzPointCloudAsset> PointCloudAsset;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SPZ")
	TObjectPtr<UNiagaraSystem> NiagaraSystemAsset;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SPZ")
	TObjectPtr<UMaterialInterface> SplatMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SPZ", meta = (ClampMin = "1", UIMin = "1"))
	int32 MaxRenderPoints = SpzNiagaraParameters::DefaultMaxRenderPoints;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SPZ", meta = (ClampMin = "0.001", UIMin = "0.001"))
	float SpriteSizeMultiplier = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SPZ", meta = (ClampMin = "0.1", UIMin = "0.1"))
	float ParticleLifetimeSeconds = SpzNiagaraParameters::DefaultParticleLifetimeSeconds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SPZ")
	FRotator RenderRotationOffset = FRotator(-90.0f, 0.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SPZ|Editor")
	bool bUseEditorViewportFiltering = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SPZ|Editor", meta = (ClampMin = "0.1", UIMin = "0.1"))
	float EditorViewportRefreshIntervalSeconds = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SPZ|Editor", meta = (ClampMin = "1", UIMin = "1"))
	int32 EditorViewportMaxRenderPoints = 12000;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SPZ|Editor", meta = (ClampMin = "0.5", UIMin = "0.5"))
	float EditorViewportFovScale = 1.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SPZ")
	bool bRefreshInConstructionScript = true;

protected:
	void ApplyPointCloudRendering();
	void ApplyRenderRotation();
	void ResetCachedPointCloudState();
	bool CanApplyPointCloud() const;
	uint32 BuildApplySignature() const;
	static void BuildLocalBounds(const TArray<FSpzSplatRenderPoint>& RenderPoints, FVector& OutOrigin, FVector& OutExtent);

#if WITH_EDITOR
	bool ShouldUseEditorViewportFiltering() const;
	bool CacheActiveEditorViewportState();
	void RefreshEditorViewportFilteredPointCloud();
#endif

	UPROPERTY(Transient)
	int32 LastAppliedParticleCount = 0;

	UPROPERTY(Transient)
	uint32 LastAppliedSignature = 0;

	UPROPERTY(Transient)
	bool bHasAppliedPointCloud = false;

#if WITH_EDITORONLY_DATA
	UPROPERTY(Transient)
	bool bHasCachedEditorViewportState = false;

	UPROPERTY(Transient)
	FVector CachedEditorViewLocationLocal = FVector::ZeroVector;

	UPROPERTY(Transient)
	FVector CachedEditorViewForwardLocal = FVector::ForwardVector;

	UPROPERTY(Transient)
	float CachedEditorHorizontalFovDegrees = 90.0f;

	UPROPERTY(Transient)
	FVector LastEditorViewportWorldLocation = FVector::ZeroVector;

	UPROPERTY(Transient)
	FRotator LastEditorViewportWorldRotation = FRotator::ZeroRotator;

	UPROPERTY(Transient)
	float LastEditorViewportWorldFovDegrees = 90.0f;

	UPROPERTY(Transient)
	double LastEditorViewportRefreshSeconds = -1.0;
#endif
};
