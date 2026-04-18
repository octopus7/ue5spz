#pragma once

#include "CoreMinimal.h"

namespace SpzGaussianActorRebuildParameters
{
	inline const FName TexPosition(TEXT("User.tex_position"));
	inline const FName TexQuat4(TEXT("User.tex_quat4"));
	inline const FName TexScaleA(TEXT("User.tex_scaleA"));
	inline const FName TexSH0(TEXT("User.tex_sh0"));
	inline const FName PointCount(TEXT("User.point_count"));
	inline const FName TextureWidth(TEXT("User.texture_width"));
	inline const FName TextureHeight(TEXT("User.texture_height"));
	inline const FName SpriteScale(TEXT("User.sprite_scale"));
	inline const FName AlbedoTint(TEXT("User.albedo_tint"));
	inline const FName InMaterial(TEXT("User.in_material"));
	inline const FName CameraRightLocal(TEXT("User.camera_right_local"));
	inline const FName CameraUpLocal(TEXT("User.camera_up_local"));

	inline constexpr float DefaultLifetimeSeconds = 3600.0f;
	inline constexpr float GaussianExtent = 3.0f;
}
