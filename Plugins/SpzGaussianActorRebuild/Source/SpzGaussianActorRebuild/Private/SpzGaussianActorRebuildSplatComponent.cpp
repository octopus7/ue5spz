#include "SpzGaussianActorRebuildSplatComponent.h"

#include "DynamicMeshBuilder.h"
#include "MaterialDomain.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "PrimitiveSceneProxy.h"
#include "SceneManagement.h"
#include "SceneInterface.h"
#include "SceneView.h"

namespace
{
	struct FSortedSplat
	{
		int32 PointIndex = INDEX_NONE;
		float Depth = 0.0f;
	};

	FColor PackLinearColor(const FLinearColor& Color)
	{
		return FColor(
			static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(Color.R * 255.0f), 0, 255)),
			static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(Color.G * 255.0f), 0, 255)),
			static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(Color.B * 255.0f), 0, 255)),
			static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(Color.A * 255.0f), 0, 255)));
	}

	void ComputeEllipseAxes(
		const FSpzGaussianActorRebuildRenderPoint& Point,
		const FVector& ViewRightLocal,
		const FVector& ViewUpLocal,
		FVector& OutAxis0,
		FVector& OutAxis1)
	{
		const FVector BasisX(Point.AxisX);
		const FVector BasisY(Point.AxisY);
		const FVector BasisZ(Point.AxisZ);

		const float DotXR = FVector::DotProduct(BasisX, ViewRightLocal);
		const float DotYR = FVector::DotProduct(BasisY, ViewRightLocal);
		const float DotZR = FVector::DotProduct(BasisZ, ViewRightLocal);
		const float DotXU = FVector::DotProduct(BasisX, ViewUpLocal);
		const float DotYU = FVector::DotProduct(BasisY, ViewUpLocal);
		const float DotZU = FVector::DotProduct(BasisZ, ViewUpLocal);

		const float Cov00 = FMath::Square(DotXR) + FMath::Square(DotYR) + FMath::Square(DotZR);
		const float Cov01 = (DotXR * DotXU) + (DotYR * DotYU) + (DotZR * DotZU);
		const float Cov11 = FMath::Square(DotXU) + FMath::Square(DotYU) + FMath::Square(DotZU);

		const float TraceHalf = (Cov00 + Cov11) * 0.5f;
		const float Delta = FMath::Sqrt(FMath::Max(0.0f, FMath::Square((Cov00 - Cov11) * 0.5f) + FMath::Square(Cov01)));
		const float EigenValue0 = FMath::Max(TraceHalf + Delta, KINDA_SMALL_NUMBER);
		const float EigenValue1 = FMath::Max(TraceHalf - Delta, KINDA_SMALL_NUMBER);

		FVector2D EigenVector0(1.0f, 0.0f);
		if (FMath::Abs(Cov01) > KINDA_SMALL_NUMBER)
		{
			EigenVector0 = FVector2D(EigenValue0 - Cov11, Cov01).GetSafeNormal();
		}
		else if (Cov11 > Cov00)
		{
			EigenVector0 = FVector2D(0.0f, 1.0f);
		}

		const FVector2D EigenVector1(-EigenVector0.Y, EigenVector0.X);
		OutAxis0 = ((ViewRightLocal * EigenVector0.X) + (ViewUpLocal * EigenVector0.Y)) * FMath::Sqrt(EigenValue0);
		OutAxis1 = ((ViewRightLocal * EigenVector1.X) + (ViewUpLocal * EigenVector1.Y)) * FMath::Sqrt(EigenValue1);

		if (OutAxis0.IsNearlyZero())
		{
			OutAxis0 = ViewRightLocal * 0.25f;
		}

		if (OutAxis1.IsNearlyZero())
		{
			OutAxis1 = ViewUpLocal * 0.25f;
		}
	}

	class FSpzGaussianActorRebuildSceneProxy final : public FPrimitiveSceneProxy
	{
	public:
		explicit FSpzGaussianActorRebuildSceneProxy(const USpzGaussianActorRebuildSplatComponent* Component)
			: FPrimitiveSceneProxy(Component)
			, RenderPoints(Component->GetRenderPoints())
			, Material(Component->GetMaterial(0) != nullptr ? Component->GetMaterial(0) : UMaterial::GetDefaultMaterial(MD_Surface))
			, MaterialRelevance(Component->GetMaterialRelevance(GetFeatureLevelShaderPlatform_Checked(GetScene().GetFeatureLevel())))
		{
			bWillEverBeLit = true;
		}

		virtual SIZE_T GetTypeHash() const override
		{
			static size_t UniquePointer;
			return reinterpret_cast<SIZE_T>(&UniquePointer);
		}

		virtual uint32 GetMemoryFootprint() const override
		{
			return sizeof(*this) + GetAllocatedSize();
		}

		uint32 GetAllocatedSize() const
		{
			return RenderPoints.GetAllocatedSize();
		}

		virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
		{
			FPrimitiveViewRelevance Result;
			Result.bDrawRelevance = IsShown(View);
			Result.bDynamicRelevance = true;
			Result.bShadowRelevance = false;
			Result.bRenderInMainPass = ShouldRenderInMainPass();
			Result.bUsesLightingChannels = GetLightingChannelMask() != GetDefaultLightingChannelMask();
			Result.bRenderCustomDepth = ShouldRenderCustomDepth();
			MaterialRelevance.SetPrimitiveViewRelevance(Result);
			return Result;
		}

		virtual bool CanBeOccluded() const override
		{
			return !MaterialRelevance.bDisableDepthTest;
		}

		virtual void GetDynamicMeshElements(
			const TArray<const FSceneView*>& Views,
			const FSceneViewFamily& ViewFamily,
			uint32 VisibilityMap,
			FMeshElementCollector& Collector) const override
		{
			if (RenderPoints.Num() == 0 || Material == nullptr)
			{
				return;
			}

			const FTransform LocalToWorldTransform(GetLocalToWorld());
			const FTransform WorldToLocalTransform = LocalToWorldTransform.Inverse();
			const FMaterialRenderProxy* MaterialRenderProxy = Material->GetRenderProxy();

			for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
			{
				if ((VisibilityMap & (1u << ViewIndex)) == 0)
				{
					continue;
				}

				const FSceneView* View = Views[ViewIndex];
				if (View == nullptr)
				{
					continue;
				}

				const FVector ViewOriginLocal = WorldToLocalTransform.TransformPosition(View->ViewMatrices.GetViewOrigin());
				const FVector ViewRightLocal = WorldToLocalTransform.TransformVectorNoScale(View->GetViewRight()).GetSafeNormal();
				const FVector ViewUpLocal = WorldToLocalTransform.TransformVectorNoScale(View->GetViewUp()).GetSafeNormal();
				const FVector ViewForwardLocal = WorldToLocalTransform.TransformVectorNoScale(View->GetViewDirection()).GetSafeNormal();
				if (ViewRightLocal.IsNearlyZero() || ViewUpLocal.IsNearlyZero() || ViewForwardLocal.IsNearlyZero())
				{
					continue;
				}

				TArray<FSortedSplat> SortedSplats;
				SortedSplats.Reserve(RenderPoints.Num());
				for (int32 PointIndex = 0; PointIndex < RenderPoints.Num(); ++PointIndex)
				{
					const float Depth = FVector::DotProduct(FVector(RenderPoints[PointIndex].Position) - ViewOriginLocal, ViewForwardLocal);
					if (Depth <= KINDA_SMALL_NUMBER)
					{
						continue;
					}

					FSortedSplat& SortedSplat = SortedSplats.AddDefaulted_GetRef();
					SortedSplat.PointIndex = PointIndex;
					SortedSplat.Depth = Depth;
				}

				if (SortedSplats.Num() == 0)
				{
					continue;
				}

				SortedSplats.Sort([](const FSortedSplat& A, const FSortedSplat& B)
				{
					return A.Depth > B.Depth;
				});

				FDynamicMeshBuilder MeshBuilder(Collector.GetFeatureLevel());
				MeshBuilder.ReserveVertices(SortedSplats.Num() * 4);
				MeshBuilder.ReserveTriangles(SortedSplats.Num() * 2);

				for (const FSortedSplat& SortedSplat : SortedSplats)
				{
					const FSpzGaussianActorRebuildRenderPoint& Point = RenderPoints[SortedSplat.PointIndex];

					FVector Axis0;
					FVector Axis1;
					ComputeEllipseAxes(Point, ViewRightLocal, ViewUpLocal, Axis0, Axis1);

					const FVector Center(Point.Position);
					const FVector Position0 = Center - Axis0 - Axis1;
					const FVector Position1 = Center + Axis0 - Axis1;
					const FVector Position2 = Center + Axis0 + Axis1;
					const FVector Position3 = Center - Axis0 + Axis1;

					const FVector TangentX = Axis0.GetSafeNormal();
					const FVector TangentY = Axis1.GetSafeNormal();
					FVector TangentZ = FVector::CrossProduct(TangentX, TangentY).GetSafeNormal();
					if (TangentZ.IsNearlyZero())
					{
						TangentZ = ViewForwardLocal;
					}

					const FColor VertexColor = PackLinearColor(Point.Color);
					FDynamicMeshVertex Vertex0(FVector3f(Position0), FVector2f(0.0f, 0.0f), VertexColor);
					FDynamicMeshVertex Vertex1(FVector3f(Position1), FVector2f(1.0f, 0.0f), VertexColor);
					FDynamicMeshVertex Vertex2(FVector3f(Position2), FVector2f(1.0f, 1.0f), VertexColor);
					FDynamicMeshVertex Vertex3(FVector3f(Position3), FVector2f(0.0f, 1.0f), VertexColor);

					Vertex0.SetTangents(FVector3f(TangentX), FVector3f(TangentY), FVector3f(TangentZ));
					Vertex1.SetTangents(FVector3f(TangentX), FVector3f(TangentY), FVector3f(TangentZ));
					Vertex2.SetTangents(FVector3f(TangentX), FVector3f(TangentY), FVector3f(TangentZ));
					Vertex3.SetTangents(FVector3f(TangentX), FVector3f(TangentY), FVector3f(TangentZ));

					const int32 BaseVertexIndex = MeshBuilder.AddVertex(Vertex0);
					MeshBuilder.AddVertex(Vertex1);
					MeshBuilder.AddVertex(Vertex2);
					MeshBuilder.AddVertex(Vertex3);
					MeshBuilder.AddTriangle(BaseVertexIndex + 0, BaseVertexIndex + 1, BaseVertexIndex + 2);
					MeshBuilder.AddTriangle(BaseVertexIndex + 0, BaseVertexIndex + 2, BaseVertexIndex + 3);
				}

				FDynamicMeshBuilderSettings Settings;
				Settings.CastShadow = false;
				Settings.bDisableBackfaceCulling = true;
				Settings.bReceivesDecals = false;
				Settings.bUseSelectionOutline = IsSelected();
				Settings.bCanApplyViewModeOverrides = true;

				MeshBuilder.GetMesh(GetLocalToWorld(), MaterialRenderProxy, SDPG_World, Settings, nullptr, ViewIndex, Collector);
			}
		}

	private:
		TArray<FSpzGaussianActorRebuildRenderPoint> RenderPoints;
		TObjectPtr<UMaterialInterface> Material;
		FMaterialRelevance MaterialRelevance;
	};
}

USpzGaussianActorRebuildSplatComponent::USpzGaussianActorRebuildSplatComponent()
{
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetGenerateOverlapEvents(false);
	SetCastShadow(false);
	bCastDynamicShadow = false;
	bAffectDistanceFieldLighting = false;
	bVisibleInRayTracing = false;
}

void USpzGaussianActorRebuildSplatComponent::SetRenderPoints(
	TArray<FSpzGaussianActorRebuildRenderPoint>&& InRenderPoints,
	const FVector& InLocalBoundsOrigin,
	const FVector& InLocalBoundsExtent)
{
	RenderPoints = MoveTemp(InRenderPoints);
	LocalBoundsOrigin = InLocalBoundsOrigin;
	LocalBoundsExtent = InLocalBoundsExtent;
	MarkRenderStateDirty();
	UpdateBounds();
}

void USpzGaussianActorRebuildSplatComponent::ClearRenderPoints()
{
	RenderPoints.Reset();
	LocalBoundsOrigin = FVector::ZeroVector;
	LocalBoundsExtent = FVector::ZeroVector;
	MarkRenderStateDirty();
	UpdateBounds();
}

FPrimitiveSceneProxy* USpzGaussianActorRebuildSplatComponent::CreateSceneProxy()
{
	if (RenderPoints.Num() == 0)
	{
		return nullptr;
	}

	return new FSpzGaussianActorRebuildSceneProxy(this);
}

FBoxSphereBounds USpzGaussianActorRebuildSplatComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	const FBox LocalBox(LocalBoundsOrigin - LocalBoundsExtent, LocalBoundsOrigin + LocalBoundsExtent);
	return FBoxSphereBounds(LocalBox).TransformBy(LocalToWorld);
}

int32 USpzGaussianActorRebuildSplatComponent::GetNumMaterials() const
{
	return 1;
}
