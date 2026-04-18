#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "SpzGaussianActorRebuildFactory.generated.h"

UCLASS()
class SPZGAUSSIANACTORREBUILDEDITOR_API USpzGaussianActorRebuildFactory : public UFactory
{
	GENERATED_BODY()

public:
	USpzGaussianActorRebuildFactory();

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
};
