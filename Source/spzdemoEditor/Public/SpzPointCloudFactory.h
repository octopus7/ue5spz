// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"
#include "SpzPointCloudFactory.generated.h"

UCLASS()
class SPZDEMOEDITOR_API USpzPointCloudFactory : public UFactory
{
	GENERATED_BODY()

public:
	USpzPointCloudFactory();

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
