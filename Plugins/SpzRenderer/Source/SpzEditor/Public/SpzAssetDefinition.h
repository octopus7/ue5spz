#pragma once

#include "AssetDefinitionDefault.h"
#include "SpzAssetDefinition.generated.h"

UCLASS()
class SPZEDITOR_API USpzAssetDefinition : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	virtual FText GetAssetDisplayName() const override;
	virtual TSoftClassPtr<UObject> GetAssetClass() const override;
	virtual FLinearColor GetAssetColor() const override;
	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override;
	virtual bool CanImport() const override { return true; }
};
