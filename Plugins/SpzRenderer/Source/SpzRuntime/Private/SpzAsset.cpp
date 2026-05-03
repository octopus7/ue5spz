#include "SpzAsset.h"

#include "EditorFramework/AssetImportData.h"

USpzAsset::USpzAsset()
{
#if WITH_EDITORONLY_DATA
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		AssetImportData = NewObject<UAssetImportData>(this, TEXT("AssetImportData"));
	}
#endif
}

void USpzAsset::InitializeFromArrays(
	TArray<FVector3f>&& InPositions,
	TArray<FVector4f>&& InRotations,
	TArray<FVector3f>&& InScales,
	TArray<FColor>&& InColors,
	TArray<float>&& InSphericalHarmonics,
	int32 InShDegree,
	bool bInAntialiased,
	const FString& InSourceFormatVersion)
{
	Positions = MoveTemp(InPositions);
	Rotations = MoveTemp(InRotations);
	Scales = MoveTemp(InScales);
	Colors = MoveTemp(InColors);
	SphericalHarmonics = MoveTemp(InSphericalHarmonics);
	ShDegree = InShDegree;
	bAntialiased = bInAntialiased;
	SourceFormatVersion = InSourceFormatVersion;
	NumSplats = Positions.Num();

	switch (ShDegree)
	{
	case 0:
		ShCoefficientsPerSplat = 0;
		break;
	case 1:
		ShCoefficientsPerSplat = 9;
		break;
	case 2:
		ShCoefficientsPerSplat = 24;
		break;
	case 3:
		ShCoefficientsPerSplat = 45;
		break;
	case 4:
		ShCoefficientsPerSplat = 72;
		break;
	default:
		ShCoefficientsPerSplat = 0;
		break;
	}

	RebuildBounds();
}

void USpzAsset::RebuildBounds()
{
	if (Positions.IsEmpty())
	{
		LocalBounds = FBoxSphereBounds(EForceInit::ForceInit);
		NumSplats = 0;
		return;
	}

	FBox Bounds(EForceInit::ForceInit);
	for (int32 Index = 0; Index < Positions.Num(); ++Index)
	{
		const FVector Scale = Scales.IsValidIndex(Index) ? FVector(Scales[Index]) : FVector::ZeroVector;
		const FVector Extent = Scale.GetAbs().GetMax() > 0.0
			? FVector(Scale.GetAbs().GetMax())
			: FVector::ZeroVector;
		Bounds += FBox::BuildAABB(FVector(Positions[Index]), Extent);
	}

	LocalBounds = FBoxSphereBounds(Bounds);
	NumSplats = Positions.Num();
}

void USpzAsset::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITORONLY_DATA
	if (!AssetImportData)
	{
		AssetImportData = NewObject<UAssetImportData>(this, TEXT("AssetImportData"));
	}
#endif

	RebuildBounds();
}
