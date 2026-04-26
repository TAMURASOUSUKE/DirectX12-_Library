#pragma once

// Math関連で共通して使うような定数をまとめる
namespace Math 
{

	constexpr int MATRIX_SIZE{ 4 }; // 行と列のサイズ

	constexpr float EPSILON{ 1e-5f }; // floatの誤差許容値
	constexpr float PI{ 3.141592653589794626f }; // 円周率
	constexpr float DEG_TO_RAD{ PI / 180.0f }; // ラジアン変換用
	constexpr float RAD_TO_DEG{ 180.0f / PI }; // デグリー変換
}