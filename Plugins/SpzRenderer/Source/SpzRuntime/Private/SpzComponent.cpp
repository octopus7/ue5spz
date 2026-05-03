#include "SpzComponent.h"

#include "Camera/PlayerCameraManager.h"
#include "Engine/CollisionProfile.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "Materials/Material.h"
#include "MeshElementCollector.h"
#include "PrimitiveSceneProxy.h"
#include "PrimitiveViewRelevance.h"
#include "SceneManagement.h"
#include "SceneView.h"
#include "SpzAsset.h"

namespace
{
struct FSpzRenderPoint
{
	FVector Position = FVector::ZeroVector;
	FLinearColor Color = FLinearColor::White;
	float Opacity = 1.0f;
};

class FSpzPointSceneProxy final : public FPrimitiveSceneProxy
{
public:
	SIZE_T GetTypeHash() const override
	{
		static size_t UniquePointer;
		return reinterpret_cast<size_t>(&UniquePointer);
	}

	explicit FSpzPointSceneProxy(const USpzComponent* Component)
		: FPrimitiveSceneProxy(Component)
		, PointSize(Component->PreviewPointSize)
		, MaxDrawSplats(Component->MaxPreviewSplats)
	{
		bWillEverBeLit = false;

		const USpzAsset* Asset = Component->SpzAsset.Get();
		if (!Asset)
		{
			return;
		}

		Points.Reserve(Asset->NumSplats);
		for (int32 Index = 0; Index < Asset->NumSplats; ++Index)
		{
			FSpzRenderPoint& Point = Points.AddDefaulted_GetRef();
			Point.Position = Asset->Positions.IsValidIndex(Index) ? FVector(Asset->Positions[Index]) : FVector::ZeroVector;
			Point.Color = Asset->Colors.IsValidIndex(Index) ? FLinearColor(Asset->Colors[Index]) : FLinearColor::White;
			Point.Opacity = Asset->Colors.IsValidIndex(Index) ? Asset->Colors[Index].A / 255.0f : 1.0f;
		}

		DrawOrder = Component->GetSortedIndices();
		if (DrawOrder.Num() != Points.Num())
		{
			DrawOrder.Reset(Points.Num());
			for (int32 Index = 0; Index < Points.Num(); ++Index)
			{
				DrawOrder.Add(Index);
			}
		}
	}

	virtual void GetDynamicMeshElements(
		const TArray<const FSceneView*>& Views,
		const FSceneViewFamily& ViewFamily,
		uint32 VisibilityMap,
		FMeshElementCollector& Collector) const override
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_SpzPointSceneProxy_GetDynamicMeshElements);

		const int32 DrawCount = MaxDrawSplats > 0
			? FMath::Min(MaxDrawSplats, DrawOrder.Num())
			: DrawOrder.Num();

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
		{
			if ((VisibilityMap & (1 << ViewIndex)) == 0)
			{
				continue;
			}

			FPrimitiveDrawInterface* PDI = Collector.GetPDI(ViewIndex);
			for (int32 DrawIndex = 0; DrawIndex < DrawCount; ++DrawIndex)
			{
				const int32 PointIndex = DrawOrder[DrawIndex];
				if (!Points.IsValidIndex(PointIndex))
				{
					continue;
				}

				const FSpzRenderPoint& Point = Points[PointIndex];
				if (Point.Opacity <= UE_KINDA_SMALL_NUMBER)
				{
					continue;
				}

				FLinearColor DrawColor = Point.Color;
				DrawColor.A *= Point.Opacity;
				PDI->DrawPoint(GetLocalToWorld().TransformPosition(Point.Position), DrawColor, PointSize, SDPG_World);
			}
		}
	}

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
	{
		FPrimitiveViewRelevance Result;
		Result.bDrawRelevance = IsShown(View);
		Result.bDynamicRelevance = true;
		Result.bNormalTranslucency = true;
		Result.bSeparateTranslucency = true;
		return Result;
	}

	virtual uint32 GetMemoryFootprint() const override
	{
		return sizeof(*this) + GetAllocatedSize();
	}

	uint32 GetAllocatedSize() const
	{
		return FPrimitiveSceneProxy::GetAllocatedSize() + Points.GetAllocatedSize() + DrawOrder.GetAllocatedSize();
	}

private:
	TArray<FSpzRenderPoint> Points;
	TArray<int32> DrawOrder;
	float PointSize = 3.0f;
	int32 MaxDrawSplats = 250000;
};
} // namespace

USpzComponent::USpzComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	bTickInEditor = true;
	bAutoActivate = true;
	bUseEditorCompositing = true;
	bIgnoreStreamingManagerUpdate = true;

	SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	SetGenerateOverlapEvents(false);
}

void USpzComponent::SetSpzAsset(USpzAsset* InAsset)
{
	if (SpzAsset == InAsset)
	{
		return;
	}

	SpzAsset = InAsset;
	RequestSortNow();
	MarkRenderStateDirty();
	UpdateBounds();
}

void USpzComponent::RequestSortNow()
{
	FVector CameraLocation;
	FVector CameraForward;
	if (!ResolveSortCamera(CameraLocation, CameraForward))
	{
		CameraForward = GetForwardVector();
		CameraLocation = GetComponentLocation() - CameraForward * 100000.0;
	}

	RebuildSortOrder(CameraLocation, CameraForward);
	MarkRenderStateDirty();
}

FPrimitiveSceneProxy* USpzComponent::CreateSceneProxy()
{
	if (!SpzAsset || SpzAsset->NumSplats <= 0)
	{
		return nullptr;
	}

	return new FSpzPointSceneProxy(this);
}

FBoxSphereBounds USpzComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	if (SpzAsset && SpzAsset->NumSplats > 0)
	{
		return SpzAsset->LocalBounds.TransformBy(LocalToWorld);
	}

	return FBoxSphereBounds(FSphere(FVector::ZeroVector, 1.0)).TransformBy(LocalToWorld);
}

void USpzComponent::OnRegister()
{
	Super::OnRegister();

	if (SpzAsset && SortedIndices.Num() != SpzAsset->NumSplats)
	{
		RequestSortNow();
	}
}

void USpzComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!SpzAsset || SpzAsset->NumSplats <= 0)
	{
		return;
	}

	if (SortMode == ESpzSortMode::None || SortMode == ESpzSortMode::OnLoadOnly || SortMode == ESpzSortMode::Manual)
	{
		return;
	}

	TimeSinceLastSort += DeltaTime;

	FVector CameraLocation;
	FVector CameraForward;
	if (!ResolveSortCamera(CameraLocation, CameraForward))
	{
		return;
	}

	bool bShouldSort = false;
	switch (SortMode)
	{
	case ESpzSortMode::EveryFrame:
		bShouldSort = true;
		break;
	case ESpzSortMode::FixedInterval:
		bShouldSort = TimeSinceLastSort >= SortInterval;
		break;
	case ESpzSortMode::CameraDeltaThreshold:
		bShouldSort = TimeSinceLastSort >= SortInterval && ShouldSortForCamera(CameraLocation, CameraForward);
		break;
	default:
		break;
	}

	if (bShouldSort)
	{
		RebuildSortOrder(CameraLocation, CameraForward);
		MarkRenderStateDirty();
	}
}

#if WITH_EDITOR
void USpzComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	RequestSortNow();
	MarkRenderStateDirty();
}
#endif

bool USpzComponent::ResolveSortCamera(FVector& OutLocation, FVector& OutForward) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const APlayerController* PlayerController = World->GetFirstPlayerController();
	if (!PlayerController || !PlayerController->PlayerCameraManager)
	{
		return false;
	}

	OutLocation = PlayerController->PlayerCameraManager->GetCameraLocation();
	OutForward = PlayerController->PlayerCameraManager->GetActorForwardVector().GetSafeNormal();
	return !OutForward.IsNearlyZero();
}

void USpzComponent::RebuildSortOrder(const FVector& CameraLocation, const FVector& CameraForward)
{
	SortedIndices.Reset();

	if (!SpzAsset)
	{
		return;
	}

	SortedIndices.Reserve(SpzAsset->NumSplats);
	for (int32 Index = 0; Index < SpzAsset->NumSplats; ++Index)
	{
		SortedIndices.Add(Index);
	}

	if (SortMode != ESpzSortMode::None)
	{
		const FTransform LocalToWorld = GetComponentTransform();
		const FVector SortForward = CameraForward.GetSafeNormal();
		SortedIndices.Sort(
			[Asset = SpzAsset.Get(), &LocalToWorld, CameraLocation, SortForward](int32 LeftIndex, int32 RightIndex)
			{
				const FVector LeftLocal = Asset->Positions.IsValidIndex(LeftIndex) ? FVector(Asset->Positions[LeftIndex]) : FVector::ZeroVector;
				const FVector RightLocal = Asset->Positions.IsValidIndex(RightIndex) ? FVector(Asset->Positions[RightIndex]) : FVector::ZeroVector;
				const FVector LeftWorld = LocalToWorld.TransformPosition(LeftLocal);
				const FVector RightWorld = LocalToWorld.TransformPosition(RightLocal);
				const double LeftDepth = FVector::DotProduct(LeftWorld - CameraLocation, SortForward);
				const double RightDepth = FVector::DotProduct(RightWorld - CameraLocation, SortForward);
				return LeftDepth > RightDepth;
			});
	}

	LastCameraLocation = CameraLocation;
	LastCameraForward = CameraForward.GetSafeNormal();
	TimeSinceLastSort = 0.0f;
	bHasLastCamera = true;
}

bool USpzComponent::ShouldSortForCamera(const FVector& CameraLocation, const FVector& CameraForward) const
{
	if (!bHasLastCamera)
	{
		return true;
	}

	const double MoveDistance = FVector::Dist(CameraLocation, LastCameraLocation);
	if (MoveDistance >= MinCameraMoveDistance)
	{
		return true;
	}

	const double Dot = FVector::DotProduct(CameraForward.GetSafeNormal(), LastCameraForward.GetSafeNormal());
	const double AngleDegrees = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Dot, -1.0, 1.0)));
	return AngleDegrees >= MinCameraAngleDegrees;
}
