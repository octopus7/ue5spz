#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SpzGaussianActorRebuildActor.generated.h"

class UMaterialInterface;
class USceneComponent;
class USpzGaussianActorRebuildAsset;
class USpzGaussianActorRebuildSplatComponent;
struct FSpzGaussianActorRebuildRenderPoint;

UCLASS(BlueprintType, Blueprintable)
class SPZGAUSSIANACTORREBUILD_API ASpzGaussianActorRebuildActor : public AActor
{
	GENERATED_BODY()

public:
	ASpzGaussianActorRebuildActor();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable, Category = "Gaussian Actor")
	void SetImportedAsset(USpzGaussianActorRebuildAsset* InImportedAsset);

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Gaussian Actor")
	void RefreshRenderState();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> DefaultSceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USpzGaussianActorRebuildSplatComponent> SplatComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gaussian Actor", meta = (ClampMin = "0.001", UIMin = "0.001", DisplayName = "SpriteScale"))
	float GaussianSpriteScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gaussian Actor")
	FLinearColor AlbedoTint = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gaussian Actor")
	bool UseRelighting = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gaussian Actor", meta = (ClampMin = "0", UIMin = "0", ToolTip = "0 renders all imported points."))
	int32 MaxRenderPoints = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gaussian Actor|Internal", AdvancedDisplay)
	TObjectPtr<USpzGaussianActorRebuildAsset> ImportedAsset;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gaussian Actor|Internal", AdvancedDisplay)
	TObjectPtr<UMaterialInterface> UnlitMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gaussian Actor|Internal", AdvancedDisplay)
	TObjectPtr<UMaterialInterface> RelightMaterial;

protected:
	void ApplyImportedAssetDefaults();
	static void BuildLocalBounds(
		const TArray<FSpzGaussianActorRebuildRenderPoint>& RenderPoints,
		FVector& OutOrigin,
		FVector& OutExtent);
};
