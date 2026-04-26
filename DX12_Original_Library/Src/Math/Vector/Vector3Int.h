#pragma once  

/// <summary>
/// Int型の三次元座標や二次元ベクトルについての機能をまとめた構造体です。
/// 内積、外積、長さ、長さの二乗、二点間ベクトル、二点間の距離、二点間の距離の二乗
/// 各演算、等価、非等価、ベクトルの長さの比較を比較演算子で行う、四方向、0,1代入、定数
/// などの機能を実装しています
/// </summary>
struct Vector3Int
{
	// 引数に何も入れない場合は0ベクトルにする
	Vector3Int() : x{ 0 }, y{ 0 }, z{ 0 } {}
	~Vector3Int() = default;

	// 引数の値で二次元座標を作成する
	Vector3Int(int _x, int _y, int _z) : x{ _x }, y{ _y }, z{ _z } {}

	// 各定数
	static const Vector3Int Zero;
	static const Vector3Int One;
	static const Vector3Int Up;
	static const Vector3Int Down;
	static const Vector3Int Right;
	static const Vector3Int Left;
	static const Vector3Int Forward;
	static const Vector3Int Back;

	// ベクトルの長さをfloatで返す
	float Length() const;

	// ベクトルの長さの2乗を返す(平方根を使わないので若干軽いです)
	int LengthSquared() const;

	// 内積を行う
	static int Dot(const Vector3Int& _vec01, const Vector3Int& _vec02);

	// 二つのベクトルの距離
	static float Distance(const Vector3Int& _startPos, const Vector3Int& _endPos);

	// 二つのベクトルの距離の二乗
	static int DistanceSquared(const Vector3Int& _startPos, const Vector3Int& _endPos);

	// 外積を行う
	static Vector3Int Cross(const Vector3Int& _vec01, const Vector3Int& _vec02);

	// 二点間のベクトル
	static Vector3Int FromTo(const Vector3Int& _startPos, const Vector3Int& _endPos);

	// 逆ベクトル
	Vector3Int operator -() const;

	// 以下演算

	// Vec3同士の加算
	Vector3Int operator +(const Vector3Int& _other) const;
	// Vec3同士の減算
	Vector3Int operator -(const Vector3Int& _other) const;
	// スカラー倍
	Vector3Int operator *(const int _value) const;
	// スカラーによる除算(0割りをすると0ベクトルを返すようにしています)
	Vector3Int operator /(const int _value) const;
	// Vec3同士の加算代入
	Vector3Int& operator +=(const Vector3Int& _other);
	// Vec3同士の減算代入
	Vector3Int& operator -=(const Vector3Int& _other);
	// スカラーとベクトルの乗算代入
	Vector3Int& operator *=(const int _value);
	// スカラーによる除算代入(0割りをすると0ベクトルを返すようにしています)
	Vector3Int& operator /=(const int _value);

	// ベクトルとスカラーの逆乗算
	friend Vector3Int operator *(const int _value, const Vector3Int& _other)
	{
		return _other * _value;
	}


	// 以下比較演算子

	// 等価
	bool operator ==(const Vector3Int& _other) const;
	// 非等価
	bool operator !=(const Vector3Int& _other) const;

	int x; // x座標
	int y; // y座標
	int z; // z座標
};