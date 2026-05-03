#pragma once

#include "ActorFactories/ActorFactory.h"
#include "SpzActorFactory.generated.h"

UCLASS(Transient)
class SPZEDITOR_API USpzActorFactory : public UActorFactory
{
	GENERATED_BODY()

public:
	USpzActorFactory(const FObjectInitializer& ObjectInitializer);

	virtual bool CanCreateActorFrom(const FAssetData& AssetData, FText& OutErrorMsg) override;
	virtual void PostSpawnActor(UObject* Asset, AActor* NewActor) override;
	virtual UObject* GetAssetFromActorInstance(AActor* ActorInstance) override;
};
