#include "SpzActor.h"

#include "SpzComponent.h"

ASpzActor::ASpzActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SpzComponent = CreateDefaultSubobject<USpzComponent>(TEXT("SpzComponent"));
	RootComponent = SpzComponent;
}
