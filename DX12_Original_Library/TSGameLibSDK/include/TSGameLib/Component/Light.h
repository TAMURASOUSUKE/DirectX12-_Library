#pragma once
#include "../Math/TSMath.h"

// 無限遠から全体へ同じ方向に届く光
struct DirectionalLight
{
	// 光線が進む方向(Yが負なら空から地面に向かって照らす)
	Vector3 direction{ 0.0f, -1.0f, 0.0f }; // 初期は真上から真下に
	// 光の色
	Vector3 color{ 1.0f, 1.0f, 1.0f };
	// 光の強さ
	float intensity{ 1.0f };
};

// シーン全体を均一に照らす最低限の光
struct AmbientLight
{
	// 光の色
	Vector3 color{ 1.0f, 1.0f, 1.0f };
	// 光の強さ
	float intensity{ 0.15f };
};

// 現在のシーンで使用するライト一式
struct SceneLight
{
	DirectionalLight directional{};
	AmbientLight ambient{};
};
