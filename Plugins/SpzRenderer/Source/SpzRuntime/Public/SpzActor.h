#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SpzActor.generated.h"

class USpzComponent;

UCLASS(BlueprintType, Blueprintable)
class SPZRUNTIME_API ASpzActor : public AActor
{
	GENERATED_BODY()

public:
	ASpzActor(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SPZ")
	TObjectPtr<USpzComponent> SpzComponent;
};
