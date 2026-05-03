#include "SpzActorFactory.h"

#include "AssetRegistry/AssetData.h"
#include "SpzActor.h"
#include "SpzAsset.h"
#include "SpzComponent.h"

#define LOCTEXT_NAMESPACE "SpzActorFactory"

USpzActorFactory::USpzActorFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = LOCTEXT("SpzActorDisplayName", "SPZ Gaussian Splat");
	NewActorClass = ASpzActor::StaticClass();
}

bool USpzActorFactory::CanCreateActorFrom(const FAssetData& AssetData, FText& OutErrorMsg)
{
	if (!AssetData.IsValid() || !AssetData.IsInstanceOf(USpzAsset::StaticClass()))
	{
		OutErrorMsg = LOCTEXT("NoSpzAsset", "A valid SPZ Gaussian Splat asset must be specified.");
		return false;
	}

	return true;
}

void USpzActorFactory::PostSpawnActor(UObject* Asset, AActor* NewActor)
{
	Super::PostSpawnActor(Asset, NewActor);

	USpzAsset* SpzAsset = CastChecked<USpzAsset>(Asset);
	ASpzActor* SpzActor = CastChecked<ASpzActor>(NewActor);
	USpzComponent* SpzComponent = SpzActor->SpzComponent;
	check(SpzComponent);

	SpzComponent->UnregisterComponent();
	SpzComponent->SetSpzAsset(SpzAsset);
	SpzComponent->RegisterComponent();
}

UObject* USpzActorFactory::GetAssetFromActorInstance(AActor* ActorInstance)
{
	ASpzActor* SpzActor = Cast<ASpzActor>(ActorInstance);
	return SpzActor && SpzActor->SpzComponent ? SpzActor->SpzComponent->SpzAsset.Get() : nullptr;
}

#undef LOCTEXT_NAMESPACE
