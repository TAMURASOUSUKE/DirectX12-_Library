#pragma once

// ハンドルをまとめる、分解して任意のデータをとりだす関数を提供する
inline constexpr int INDEX_BITS{ 16 }; // bitpackのインデックス部分
inline constexpr int INDEX_MASK{ (1 << INDEX_BITS) - 1 }; // 下位16ビットのマスク

// レジストリ操作

/// <summary>
/// 合成する
/// </summary>
/// <param name="_index">インデックス値</param>
/// <param name="_gen">世代値</param>
/// <returns>int型につめられたハンドル</returns>
constexpr int Pack(int _index, int _generation)
{
	return (_generation << INDEX_BITS) | (_index & INDEX_MASK); // indexが16bitを超えたら0にする
}

/// <summary>
/// Index値を取り出す
/// </summary>
/// <param name="_packed">Packされたハンドル</param>
/// <returns>Index値</returns>
constexpr int UnpackIndex(int _packed)
{
	return _packed & INDEX_MASK;
}

/// <summary>
/// 世代値を取り出す
/// </summary>
/// <param name="_packed">Packされたハンドル</param>
/// <returns>世代値</returns>
constexpr int UnpackGen(int _packed)
{
	return _packed >> INDEX_BITS;
}
