#pragma once

#include "CoreMinimal.h"
#include "EditorReimportHandler.h"
#include "Factories/Factory.h"
#include "SpzAssetFactory.generated.h"

class USpzAsset;

UCLASS()
class SPZEDITOR_API USpzAssetFactory : public UFactory, public FReimportHandler
{
	GENERATED_BODY()

public:
	USpzAssetFactory();

	UPROPERTY(EditAnywhere, Category = "SPZ")
	float ImportScale = 100.0f;

	UPROPERTY(EditAnywhere, Category = "SPZ")
	bool bConvertCoordinatesToUnreal = true;

	virtual bool FactoryCanImport(const FString& Filename) override;
	virtual UObject* FactoryCreateFile(
		UClass* InClass,
		UObject* InParent,
		FName InName,
		EObjectFlags Flags,
		const FString& Filename,
		const TCHAR* Parms,
		FFeedbackContext* Warn,
		bool& bOutOperationCanceled) override;

	virtual bool CanReimport(UObject* Obj, TArray<FString>& OutFilenames) override;
	virtual void SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths) override;
	virtual EReimportResult::Type Reimport(UObject* Obj) override;
	virtual int32 GetPriority() const override;

private:
	bool ImportIntoAsset(USpzAsset* Asset, const FString& Filename, FFeedbackContext* Warn) const;
};
