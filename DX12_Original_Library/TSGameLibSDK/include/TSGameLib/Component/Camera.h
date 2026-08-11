#pragma once
#include "Transform.h"

// 3D空間を映すカメラの設定を持つ
struct Camera
{
	Transform transform{};

	// 縦方向の視野角
	float fieldOfViewY{ 60.0f * Math::DEG_TO_RAD };

	// この距離より近いものを描画しない
	float nearClip{ 0.1f };
	// この距離より遠い物を描画しない
	float farClip{ 1000.0f };
};
