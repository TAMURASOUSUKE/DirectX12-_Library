#pragma once

/// <summary>
/// Int型の二次元座標や二次元ベクトルについての機能をまとめた構造体です。
/// 内積、外積、長さ、長さの二乗、二点間ベクトル、二点間の距離、二点間の距離の二乗
/// 各演算、等価、非等価、ベクトルの長さの比較を比較演算子で行う、四方向、0,1代入、定数
/// などの機能を実装しています
/// </summary>
struct Vector2Int
{
	// 引数に何も入れない場合は0ベクトルにする
	Vector2Int() : x{ 0 }, y{ 0 } {}
	~Vector2Int() = default;

	// 引数の値で二次元座標を作成する
	Vector2Int(int _x, int _y) : x{ _x }, y{ _y } {}

	// 各定数
	static const Vector2Int Zero;
	static const Vector2Int One;
	static const Vector2Int Up;
	static const Vector2Int Down;
	static const Vector2Int Right;
	static const Vector2Int Left;

	// ベクトルの長さをfloatで返す
	float Length() const;

	// ベクトルの長さの2乗を返す(平方根を使わないので若干軽いです)
	int LengthSquared() const;

	// 内積を行う
	static int Dot(const Vector2Int& _vec01, const Vector2Int& _vec02);

	// 外積を行う(2次元ベクトルの外積なので正なら反時計回り、負なら時計回りとなる)
	static int Cross(const Vector2Int& _vec01, const Vector2Int& _vec02);

	// 二つのベクトルの距離
	static float Distance(const Vector2Int& _startPos, const Vector2Int& _endPos);

	// 二つのベクトルの距離の二乗
	static int DistanceSquared(const Vector2Int& _startPos, const Vector2Int& _endPos);

	// 二点間のベクトル
	static Vector2Int FromTo(const Vector2Int& _startPos, const Vector2Int& _endPos);

	// 逆ベクトル
	Vector2Int operator -() const;

	// 以下演算

	// Vec2同士の加算
	Vector2Int operator +(const Vector2Int& _other) const;
	// Vec2同士の減算
	Vector2Int operator -(const Vector2Int& _other) const;
	// スカラー倍
	Vector2Int operator *(const int _value) const;
	// スカラーによる除算(0割りをすると0ベクトルを返すようにしています)
	Vector2Int operator /(const int _value) const;
	// Vec2同士の加算代入
	Vector2Int& operator +=(const Vector2Int& _other);
	// Vec2同士の減算代入
	Vector2Int& operator -=(const Vector2Int& _other);
	// スカラーとベクトルの乗算代入
	Vector2Int& operator *=(const int _value);
	// スカラーによる除算代入(0割りをすると0ベクトルを返すようにしています)
	Vector2Int& operator /=(const int _value);

	// ベクトルとスカラーの逆乗算
	friend Vector2Int operator *(const int _value, const Vector2Int& _other)
	{
		return _other * _value;
	}


	// 以下比較演算子

	// 等価
	bool operator ==(const Vector2Int& _other) const;
	// 非等価
	bool operator !=(const Vector2Int& _other) const;

	int x; // x座標
	int y; // y座標
};