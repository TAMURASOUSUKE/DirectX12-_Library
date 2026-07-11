#pragma once
// ここでは各Handleが汎用的に扱う情報を定義する

 // Handle作成時にPassKeyIdiomを採用したため生成を許すクラスであるReosurceManager類を前方宣言
class GraphicsResourceManager;
class SoundResourceManager;

// bitpackのインデックス部分
constexpr int INDEX_BITS{ 16 };

// 下位16ビットのマスク
constexpr int INDEX_MASK{ (1 << INDEX_BITS) - 1 }; 

// レジストリ操作

/// <summary>
/// 合成する
/// </summary>
/// <param name="_index">インデックス値</param>
/// <param name="_gen">世代値</param>
/// <returns>int型につめられたハンドル</returns>
constexpr int Pack(int _index, int _gen) { return (_gen << INDEX_BITS) | (_index & INDEX_MASK); } // indexが16bitを超えたら0にする

/// <summary>
/// Index値を取り出す
/// </summary>
/// <param name="_packed">Packされたハンドル</param>
/// <returns>Index値</returns>
constexpr int UnpackIndex(int _packed) { return _packed & INDEX_MASK; }

/// <summary>
/// 世代値を取り出す
/// </summary>
/// <param name="_packed">Packされたハンドル</param>
/// <returns>世代値</returns>
constexpr int UnpackGen(int _packed) { return _packed >> INDEX_BITS; }

// 各Handleクラスが必要とする鍵(PassKeyIdiom)
class PassKey
{
	PassKey() = default; // デフォルトコンストラクタ
	friend class GraphicsResourceManager; // GraphicsReosurceManagerが鍵を作れる
	friend class SoundResourceManager;  // SoundResourceManagerが鍵を作れる
};