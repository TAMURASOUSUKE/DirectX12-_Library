#pragma once

class ResourceManager; // Handle作成時にPassKeyIdiomを採用したため生成を許すクラスであるReosurceManagerを前方宣言

// ここでは各Handleが汎用的に扱う情報を定義する

// 各Handleクラスが必要とする鍵(PassKeyIdiom)
struct PassKey
{
private:
	PassKey() = default; // デフォルトコンストラクタ
	friend class ResourceManager; // ReosurceManagerのみが鍵を作れる
};