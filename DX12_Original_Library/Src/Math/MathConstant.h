#pragma once
#include <cmath>
#include "Vector/Vector3.h"

// Math関連で共通して使うような定数をまとめる
namespace Math 
{

	constexpr int MATRIX_SIZE{ 4 }; // 行と列のサイズ

	constexpr float EPSILON{ 1e-5f }; // floatの誤差許容値
	constexpr float PI{ 3.141592653589794626f }; // 円周率
	constexpr float DEG_TO_RAD{ PI / 180.0f }; // ラジアン変換用
	constexpr float RAD_TO_DEG{ 180.0f / PI }; // デグリー変換

	// 角度の正規化
	inline float NormalizeAngle(float _angle)
	{
		_angle = std::fmod(_angle, 2.0f * PI);
		// 負の数の場合の対処
		if (_angle < 0.0f) _angle += 2.0f * PI;
		return _angle;
	}

	// 角度の正規化Vector3版(全ての要素に対して正規化します)
	inline Vector3 NormalizeAngle(Vector3 _angle)
	{
		_angle.x = NormalizeAngle(_angle.x);
		_angle.y = NormalizeAngle(_angle.y);
		_angle.z = NormalizeAngle(_angle.z);
		return _angle;
	}
}