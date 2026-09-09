#pragma once
#include "../Math/TSMath.h"

// 平行光源による影の描画範囲を決定する
struct DirectionalShadowSettings
{
	//　影を生成する範囲の中心
	Vector3 focusPosition{ Vector3::Zero };
	// 光源をFocusPositionからどれだけ離して配置するか(描画用カメラの仮想的な距離として扱う)
	float lightDistance{ 50.0f };
	// 正射影で移すワールド空間の横幅
	float width{ 50.0f };
	// 正射影で移すワールド空間の縦幅
	float height{ 50.0f };
	// 光源カメラから描画を始める距離
	float nearClip{ 0.1f };
	// 光源カメラから描画を終了する距離
	float farClip{ 100.0f };
};
