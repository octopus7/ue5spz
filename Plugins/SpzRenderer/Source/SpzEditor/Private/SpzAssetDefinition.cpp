#include "SpzAssetDefinition.h"

#include "AssetDefinition.h"
#include "SpzAsset.h"

#define LOCTEXT_NAMESPACE "SpzAssetDefinition"

FText USpzAssetDefinition::GetAssetDisplayName() const
{
	return LOCTEXT("SpzAssetDisplayName", "SPZ Gaussian Splat");
}

TSoftClassPtr<UObject> USpzAssetDefinition::GetAssetClass() const
{
	return USpzAsset::StaticClass();
}

FLinearColor USpzAssetDefinition::GetAssetColor() const
{
	return FLinearColor(0.16f, 0.53f, 0.72f);
}

TConstArrayView<FAssetCategoryPath> USpzAssetDefinition::GetAssetCategories() const
{
	static const auto Categories = { EAssetCategoryPaths::FX };
	return Categories;
}

#undef LOCTEXT_NAMESPACE
