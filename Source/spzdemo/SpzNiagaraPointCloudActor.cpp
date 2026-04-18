// Copyright Epic Games, Inc. All Rights Reserved.

#include "SpzNiagaraPointCloudActor.h"

#include "Components/SceneComponent.h"
#include "HAL/PlatformTime.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraComponent.h"
#include "SpzGaussianSplatComponent.h"
#include "SpzPointCloudAsset.h"

#if WITH_EDITOR
#include "Editor.h"
#include "LevelEditorViewport.h"
#endif

ASpzNiagaraPointCloudActor::ASpzNiagaraPointCloudActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	RenderRoot = CreateDefaultSubobject<USceneComponent>(TEXT("RenderRoot"));
	RenderRoot->SetupAttachment(SceneRoot);
	RenderRoot->SetRelativeRotation(RenderRotationOffset);

	NiagaraComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("NiagaraComponent"));
	NiagaraComponent->SetupAttachment(RenderRoot);
	NiagaraComponent->SetAutoActivate(false);
	NiagaraComponent->SetVisibility(false);
	NiagaraComponent->SetHiddenInGame(true);

	SplatComponent = CreateDefaultSubobject<USpzGaussianSplatComponent>(TEXT("SplatComponent"));
	SplatComponent->SetupAttachment(RenderRoot);
	SplatComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ASpzNiagaraPointCloudActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyRenderRotation();

#if WITH_EDITOR
	CacheActiveEditorViewportState();
#endif

	if (bRefreshInConstructionScript)
	{
		ApplyPointCloudRendering();
	}
}

void ASpzNiagaraPointCloudActor::BeginPlay()
{
	Super::BeginPlay();
	ApplyRenderRotation();
	ApplyPointCloudRendering();
}

void ASpzNiagaraPointCloudActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

#if WITH_EDITOR
	RefreshEditorViewportFilteredPointCloud();
#endif
}

bool ASpzNiagaraPointCloudActor::ShouldTickIfViewportsOnly() const
{
	return true;
}

void ASpzNiagaraPointCloudActor::RefreshPointCloud()
{
	ApplyPointCloudRendering();
}

void ASpzNiagaraPointCloudActor::SetPointCloudAsset(USpzPointCloudAsset* NewPointCloudAsset)
{
	PointCloudAsset = NewPointCloudAsset;
	ResetCachedPointCloudState();
	ApplyPointCloudRendering();
}

bool ASpzNiagaraPointCloudActor::CanApplyPointCloud() const
{
	return HasAnyFlags(RF_ClassDefaultObject) == false
		&& SplatComponent != nullptr
		&& PointCloudAsset != nullptr
		&& PointCloudAsset->HasRenderableData();
}

void ASpzNiagaraPointCloudActor::ApplyRenderRotation()
{
	if (RenderRoot != nullptr)
	{
		RenderRoot->SetRelativeRotation(RenderRotationOffset);
	}
}

void ASpzNiagaraPointCloudActor::ResetCachedPointCloudState()
{
	LastAppliedParticleCount = 0;
	LastAppliedSignature = 0;
	bHasAppliedPointCloud = false;
}

uint32 ASpzNiagaraPointCloudActor::BuildApplySignature() const
{
	uint32 Signature = 0;
	Signature = HashCombineFast(Signature, GetTypeHash(PointCloudAsset.Get()));
	Signature = HashCombineFast(Signature, GetTypeHash(SplatMaterial.Get()));
	Signature = HashCombineFast(Signature, GetTypeHash(MaxRenderPoints));
	Signature = HashCombineFast(Signature, GetTypeHash(SpriteSizeMultiplier));

	if (PointCloudAsset != nullptr)
	{
		Signature = HashCombineFast(Signature, GetTypeHash(PointCloudAsset->GetRenderDataVersion()));
		Signature = HashCombineFast(Signature, GetTypeHash(PointCloudAsset->GetStoredPointCount()));
	}

	return Signature;
}

#if WITH_EDITOR
bool ASpzNiagaraPointCloudActor::ShouldUseEditorViewportFiltering() const
{
	return bUseEditorViewportFiltering
		&& GetWorld() != nullptr
		&& GetWorld()->WorldType == EWorldType::Editor;
}

bool ASpzNiagaraPointCloudActor::CacheActiveEditorViewportState()
{
	if (!ShouldUseEditorViewportFiltering() || RenderRoot == nullptr || GEditor == nullptr)
	{
		bHasCachedEditorViewportState = false;
		return false;
	}

	FLevelEditorViewportClient* ViewportClient = GCurrentLevelEditingViewportClient;
	if (ViewportClient == nullptr || !ViewportClient->IsPerspective() || !ViewportClient->IsVisible())
	{
		for (FLevelEditorViewportClient* CandidateViewportClient : GEditor->GetLevelViewportClients())
		{
			if (CandidateViewportClient != nullptr && CandidateViewportClient->IsPerspective() && CandidateViewportClient->IsVisible())
			{
				ViewportClient = CandidateViewportClient;
				break;
			}
		}
	}

	if (ViewportClient == nullptr)
	{
		bHasCachedEditorViewportState = false;
		return false;
	}

	const FVector ViewLocationWorld = ViewportClient->GetViewLocation();
	const FRotator ViewRotationWorld = ViewportClient->GetViewRotation();
	const float HorizontalFovDegrees = ViewportClient->ViewFOV;
	const FTransform WorldToLocal = RenderRoot->GetComponentTransform().Inverse();

	CachedEditorViewLocationLocal = WorldToLocal.TransformPosition(ViewLocationWorld);
	CachedEditorViewForwardLocal = WorldToLocal.TransformVectorNoScale(ViewRotationWorld.Vector()).GetSafeNormal();
	if (CachedEditorViewForwardLocal.IsNearlyZero())
	{
		CachedEditorViewForwardLocal = FVector::ForwardVector;
	}
	CachedEditorHorizontalFovDegrees = HorizontalFovDegrees;
	bHasCachedEditorViewportState = true;

	const bool bCameraChanged =
		FVector::DistSquared(ViewLocationWorld, LastEditorViewportWorldLocation) > FMath::Square(50.0f)
		|| !ViewRotationWorld.Equals(LastEditorViewportWorldRotation, 1.0f)
		|| !FMath::IsNearlyEqual(HorizontalFovDegrees, LastEditorViewportWorldFovDegrees, 0.5f);

	LastEditorViewportWorldLocation = ViewLocationWorld;
	LastEditorViewportWorldRotation = ViewRotationWorld;
	LastEditorViewportWorldFovDegrees = HorizontalFovDegrees;
	return bCameraChanged;
}

void ASpzNiagaraPointCloudActor::RefreshEditorViewportFilteredPointCloud()
{
	if (!ShouldUseEditorViewportFiltering())
	{
		return;
	}

	const double CurrentTimeSeconds = FPlatformTime::Seconds();
	if (LastEditorViewportRefreshSeconds >= 0.0
		&& (CurrentTimeSeconds - LastEditorViewportRefreshSeconds) < EditorViewportRefreshIntervalSeconds)
	{
		return;
	}

	const bool bCameraChanged = CacheActiveEditorViewportState();
	if (!bCameraChanged && bHasAppliedPointCloud)
	{
		LastEditorViewportRefreshSeconds = CurrentTimeSeconds;
		return;
	}

	LastEditorViewportRefreshSeconds = CurrentTimeSeconds;
	ResetCachedPointCloudState();
	ApplyPointCloudRendering();
}
#endif

void ASpzNiagaraPointCloudActor::BuildLocalBounds(const TArray<FSpzSplatRenderPoint>& RenderPoints, FVector& OutOrigin, FVector& OutExtent)
{
	if (RenderPoints.Num() == 0)
	{
		OutOrigin = FVector::ZeroVector;
		OutExtent = FVector::ZeroVector;
		return;
	}

	FBox LocalBox(EForceInit::ForceInit);
	for (const FSpzSplatRenderPoint& RenderPoint : RenderPoints)
	{
		const FVector Center(RenderPoint.Position);
		const FVector AxisExtent(
			FMath::Abs(RenderPoint.AxisX.X) + FMath::Abs(RenderPoint.AxisY.X) + FMath::Abs(RenderPoint.AxisZ.X),
			FMath::Abs(RenderPoint.AxisX.Y) + FMath::Abs(RenderPoint.AxisY.Y) + FMath::Abs(RenderPoint.AxisZ.Y),
			FMath::Abs(RenderPoint.AxisX.Z) + FMath::Abs(RenderPoint.AxisY.Z) + FMath::Abs(RenderPoint.AxisZ.Z));

		LocalBox += Center - AxisExtent;
		LocalBox += Center + AxisExtent;
	}

	OutOrigin = LocalBox.GetCenter();
	OutExtent = LocalBox.GetExtent();
}

void ASpzNiagaraPointCloudActor::ApplyPointCloudRendering()
{
	if (HasAnyFlags(RF_ClassDefaultObject) || SplatComponent == nullptr)
	{
		return;
	}

	if (SplatMaterial != nullptr && SplatComponent->GetMaterial(0) != SplatMaterial)
	{
		SplatComponent->SetMaterial(0, SplatMaterial);
	}

	if (!CanApplyPointCloud())
	{
		ResetCachedPointCloudState();
		SplatComponent->ClearRenderPoints();
		return;
	}

	const uint32 ApplySignature = BuildApplySignature();
	const bool bNeedsDataUpload = !bHasAppliedPointCloud || LastAppliedSignature != ApplySignature;
	if (!bNeedsDataUpload)
	{
		return;
	}

	TArray<FSpzSplatRenderPoint> RenderPoints;

#if WITH_EDITOR
	if (ShouldUseEditorViewportFiltering() && bHasCachedEditorViewportState)
	{
		const int32 EffectiveMaxRenderPoints = FMath::Min(MaxRenderPoints, FMath::Max(1, EditorViewportMaxRenderPoints));
		PointCloudAsset->BuildRenderPointsForView(
			EffectiveMaxRenderPoints,
			SpriteSizeMultiplier,
			CachedEditorViewLocationLocal,
			CachedEditorViewForwardLocal,
			CachedEditorHorizontalFovDegrees,
			EditorViewportFovScale,
			RenderPoints);
	}
	else
#endif
	{
		const int32 EffectiveMaxRenderPoints =
#if WITH_EDITOR
			ShouldUseEditorViewportFiltering()
			? FMath::Min(MaxRenderPoints, FMath::Max(1, EditorViewportMaxRenderPoints))
			:
#endif
			MaxRenderPoints;

		PointCloudAsset->BuildRenderPoints(EffectiveMaxRenderPoints, SpriteSizeMultiplier, RenderPoints);
	}

	LastAppliedParticleCount = RenderPoints.Num();
	if (LastAppliedParticleCount == 0)
	{
		ResetCachedPointCloudState();
		SplatComponent->ClearRenderPoints();
		return;
	}

	FVector LocalBoundsOrigin;
	FVector LocalBoundsExtent;
	BuildLocalBounds(RenderPoints, LocalBoundsOrigin, LocalBoundsExtent);
	SplatComponent->SetRenderPoints(MoveTemp(RenderPoints), LocalBoundsOrigin, LocalBoundsExtent);

	LastAppliedSignature = ApplySignature;
	bHasAppliedPointCloud = true;
}
