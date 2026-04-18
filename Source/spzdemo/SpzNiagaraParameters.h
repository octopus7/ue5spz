// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace SpzNiagaraParameters
{
	inline const FName PointCount(TEXT("User.SpzPointCount"));
	inline const FName Positions(TEXT("User.SpzPositions"));
	inline const FName Colors(TEXT("User.SpzColors"));
	inline const FName SpriteSizes(TEXT("User.SpzSpriteSizes"));
	inline const FName ParticleLifetime(TEXT("User.SpzParticleLifetime"));

	inline constexpr int32 DefaultMaxRenderPoints = 100000;
	inline constexpr float DefaultParticleLifetimeSeconds = 3600.0f;
}
