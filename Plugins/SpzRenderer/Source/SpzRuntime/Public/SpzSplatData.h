#pragma once

#include "CoreMinimal.h"
#include "SpzSplatData.generated.h"

UENUM(BlueprintType)
enum class ESpzSortMode : uint8
{
	None UMETA(DisplayName = "None"),
	OnLoadOnly UMETA(DisplayName = "On Load Only"),
	FixedInterval UMETA(DisplayName = "Fixed Interval"),
	CameraDeltaThreshold UMETA(DisplayName = "Camera Delta Threshold"),
	EveryFrame UMETA(DisplayName = "Every Frame"),
	Manual UMETA(DisplayName = "Manual")
};

USTRUCT(BlueprintType)
struct SPZRUNTIME_API FSpzSplat
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SPZ")
	FVector Position = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SPZ")
	FQuat Rotation = FQuat::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SPZ")
	FVector Scale = FVector::OneVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SPZ")
	FLinearColor Color = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SPZ", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Opacity = 1.0f;
};
