#include "SpzGaussianActorRebuildActor.h"

#include "Components/SceneComponent.h"
#include "Materials/MaterialInterface.h"
#include "SpzGaussianActorRebuildAsset.h"
#include "SpzGaussianActorRebuildSplatComponent.h"

ASpzGaussianActorRebuildActor::ASpzGaussianActorRebuildActor()
{
	PrimaryActorTick.bCanEverTick = false;

	DefaultSceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("DefaultSceneRoot"));
	SetRootComponent(DefaultSceneRoot);

	SplatComponent = CreateDefaultSubobject<USpzGaussianActorRebuildSplatComponent>(TEXT("SplatComponent"));
	SplatComponent->SetupAttachment(DefaultSceneRoot);
	SplatComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ASpzGaussianActorRebuildActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RefreshRenderState();
}

void ASpzGaussianActorRebuildActor::BeginPlay()
{
	Super::BeginPlay();
	RefreshRenderState();
}

void ASpzGaussianActorRebuildActor::SetImportedAsset(USpzGaussianActorRebuildAsset* InImportedAsset)
{
	ImportedAsset = InImportedAsset;
	RefreshRenderState();
}

void ASpzGaussianActorRebuildActor::ApplyImportedAssetDefaults()
{
	if (ImportedAsset == nullptr)
	{
		return;
	}

	UnlitMaterial = ImportedAsset->UnlitMaterial;
	RelightMaterial = ImportedAsset->RelightMaterial;
}

void ASpzGaussianActorRebuildActor::BuildLocalBounds(
	const TArray<FSpzGaussianActorRebuildRenderPoint>& RenderPoints,
	FVector& OutOrigin,
	FVector& OutExtent)
{
	if (RenderPoints.Num() == 0)
	{
		OutOrigin = FVector::ZeroVector;
		OutExtent = FVector::ZeroVector;
		return;
	}

	FBox LocalBox(EForceInit::ForceInit);
	for (const FSpzGaussianActorRebuildRenderPoint& RenderPoint : RenderPoints)
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

void ASpzGaussianActorRebuildActor::RefreshRenderState()
{
	if (HasAnyFlags(RF_ClassDefaultObject) || SplatComponent == nullptr)
	{
		return;
	}

	ApplyImportedAssetDefaults();

	UMaterialInterface* ActiveMaterial = UseRelighting && RelightMaterial != nullptr ? RelightMaterial : UnlitMaterial;
	if (ActiveMaterial != nullptr && SplatComponent->GetMaterial(0) != ActiveMaterial)
	{
		SplatComponent->SetMaterial(0, ActiveMaterial);
	}

	if (ImportedAsset == nullptr || !ImportedAsset->HasRenderableData())
	{
		SplatComponent->ClearRenderPoints();
		return;
	}

	const int32 RequestedPointCount = MaxRenderPoints > 0 ? MaxRenderPoints : ImportedAsset->GetStoredPointCount();
	TArray<FSpzGaussianActorRebuildRenderPoint> RenderPoints;
	ImportedAsset->BuildRenderPoints(RequestedPointCount, GaussianSpriteScale, RenderPoints);
	for (FSpzGaussianActorRebuildRenderPoint& RenderPoint : RenderPoints)
	{
		RenderPoint.Color *= AlbedoTint;
	}

	if (RenderPoints.Num() == 0)
	{
		SplatComponent->ClearRenderPoints();
		return;
	}

	FVector LocalBoundsOrigin;
	FVector LocalBoundsExtent;
	BuildLocalBounds(RenderPoints, LocalBoundsOrigin, LocalBoundsExtent);
	SplatComponent->SetRenderPoints(MoveTemp(RenderPoints), LocalBoundsOrigin, LocalBoundsExtent);
}
