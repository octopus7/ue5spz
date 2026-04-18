// Copyright Epic Games, Inc. All Rights Reserved.

#include "SpzNiagaraPointCloudActor.h"

#include "Components/SceneComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraDataInterfaceArrayFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "SpzPointCloudAsset.h"

ASpzNiagaraPointCloudActor::ASpzNiagaraPointCloudActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	NiagaraComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("NiagaraComponent"));
	NiagaraComponent->SetupAttachment(SceneRoot);
	NiagaraComponent->SetAutoActivate(false);
}

void ASpzNiagaraPointCloudActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (bRefreshInConstructionScript)
	{
		ApplyPointCloudToNiagara(false);
	}
}

void ASpzNiagaraPointCloudActor::BeginPlay()
{
	Super::BeginPlay();
	ApplyPointCloudToNiagara(true);
}

void ASpzNiagaraPointCloudActor::RefreshPointCloud()
{
	ApplyPointCloudToNiagara(true);
}

void ASpzNiagaraPointCloudActor::SetPointCloudAsset(USpzPointCloudAsset* NewPointCloudAsset)
{
	PointCloudAsset = NewPointCloudAsset;
	ApplyPointCloudToNiagara(true);
}

bool ASpzNiagaraPointCloudActor::CanApplyPointCloud() const
{
	return HasAnyFlags(RF_ClassDefaultObject) == false
		&& NiagaraComponent != nullptr
		&& PointCloudAsset != nullptr
		&& PointCloudAsset->HasRenderableData()
		&& NiagaraComponent->GetAsset() != nullptr;
}

void ASpzNiagaraPointCloudActor::ApplyPointCloudToNiagara(bool bForceResetSystem)
{
	if (HasAnyFlags(RF_ClassDefaultObject) || NiagaraComponent == nullptr)
	{
		return;
	}

	if (NiagaraSystemAsset != nullptr && NiagaraComponent->GetAsset() != NiagaraSystemAsset)
	{
		NiagaraComponent->SetAsset(NiagaraSystemAsset);
	}

	if (!CanApplyPointCloud())
	{
		LastAppliedParticleCount = 0;
		NiagaraComponent->Deactivate();
		NiagaraComponent->ClearSystemFixedBounds();
		return;
	}

	TArray<FVector> RenderPositions;
	TArray<FLinearColor> RenderColors;
	TArray<FVector2D> RenderSpriteSizes;
	PointCloudAsset->BuildRenderArrays(MaxRenderPoints, SpriteSizeMultiplier, RenderPositions, RenderColors, RenderSpriteSizes);

	LastAppliedParticleCount = RenderPositions.Num();
	if (LastAppliedParticleCount == 0)
	{
		NiagaraComponent->Deactivate();
		NiagaraComponent->ClearSystemFixedBounds();
		return;
	}

	float MaxHalfSpriteSize = 0.0f;
	for (const FVector2D& SpriteSize : RenderSpriteSizes)
	{
		MaxHalfSpriteSize = FMath::Max(MaxHalfSpriteSize, FMath::Max(SpriteSize.X, SpriteSize.Y) * 0.5f);
	}

	const FVector Padding(MaxHalfSpriteSize);
	const FBox LocalBounds(
		PointCloudAsset->BoundsOrigin - PointCloudAsset->BoundsExtent - Padding,
		PointCloudAsset->BoundsOrigin + PointCloudAsset->BoundsExtent + Padding);

	NiagaraComponent->SetSystemFixedBounds(LocalBounds);
	NiagaraComponent->SetVariableInt(SpzNiagaraParameters::PointCount, LastAppliedParticleCount);
	NiagaraComponent->SetVariableFloat(SpzNiagaraParameters::ParticleLifetime, ParticleLifetimeSeconds);
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayPosition(NiagaraComponent, SpzNiagaraParameters::Positions, RenderPositions);
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayColor(NiagaraComponent, SpzNiagaraParameters::Colors, RenderColors);
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector2D(NiagaraComponent, SpzNiagaraParameters::SpriteSizes, RenderSpriteSizes);

	if (bForceResetSystem || !NiagaraComponent->IsActive())
	{
		NiagaraComponent->ReinitializeSystem();
	}

	NiagaraComponent->Activate(true);
}
