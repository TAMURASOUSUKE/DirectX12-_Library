#pragma once
#include "Vector3.h" // 変換用

/// <summary>
/// 16バイト境界で配置される四次元座標や四次元ベクトルについての機能をまとめた構造体です。
/// 内積、長さ、長さの二乗、 線形補完、二点間ベクトル、二点間の距離、二点間の距離の二乗
/// 各演算、等価、非等価、ベクトルの長さの比較を比較演算子で行う、四方向、0,1代入定数
/// などの機能を実装しています
/// </summary>
struct alignas(16) Vector4
{
	// デフォルトコンストラクタでは0ベクトルで初期化する(Vector3の拡張として捉え、wは0.0fで方向ベクトルとして最初は初期化します)
	constexpr Vector4() : x{ 0.0f }, y{ 0.0f }, z{ 0.0f }, w{ 0.0f } {}
	// 3次元座標を引数で設定するコンストラクタ(Vector3の拡張として捉え、wは0.0fで方向ベクトルとして最初は初期化します)
	constexpr Vector4(float _x, float _y, float _z, float _w = 0.0f) : x{ _x }, y{ _y }, z{ _z }, w{ _w } {}
	// 三次元ベクトルとw値で初期化するコンストラクタ(Vector3の拡張として捉え、wは0.0fで方向ベクトルとして最初は初期化します)
	constexpr Vector4(Vector3 _vec, float _w = 0.0f) : x{_vec.x}, y{_vec.y}, z{_vec.z}, w{_w} {}
	// デフォルトデストラクタ
	~Vector4() = default;

	// 各定数
	static const Vector4 Zero;
	static const Vector4 One;
	static const Vector4 Up;
	static const Vector4 Down;
	static const Vector4 Right;
	static const Vector4 Left;
	static const Vector4 Forward;
	static const Vector4 Back;

	// 内積を行う
	static float Dot(const Vector4& _vec01, const Vector4& _vec02);

	// ベクトルの長さをfloatで返す
	float Length() const;

	// ベクトルの長さの2乗を返す(平方根を使わないので若干軽いです)
	float LengthSquared() const;

	// ベクトルを正規化する(0ベクトルはそのまま)
	void Normalize();

	// Vector4からVector3への変換(wを捨てる)
	Vector3 ToVec3() const;

	// 正規化されたベクトルを返す(0ベクトルならそのまま)
	static Vector4 Normalized(Vector4 _vec);

	// 二点間のベクトル
	static Vector4 FromTo(const Vector4& _startPos, const Vector4& _endPos);

	// 二つのベクトルの距離
	static float Distance(const Vector4& _startPos, const Vector4& _endPos);

	// 二つのベクトルの距離の二乗
	static float DistanceSquared(const Vector4& _startPos, const Vector4& _endPos);

	// 二点間の二次元座標を第三引数の値で補完する(第三引数は0から1の間の数で渡してください)
	static Vector4 Lerp(const Vector4& _startPos, const Vector4& _endPos, float _completionValue);

	// Vector3を方向ベクトルとしてVector4に変換する(w = 0)
	static Vector4 FromDirection(const Vector3& _dir);

	// Vector3を位置座標としてVector4に変換する(w = 1)
	static Vector4 FromPosition(const Vector3& _pos);

	// 逆ベクトル
	Vector4 operator -() const;

	// 以下演算

	// Vec4同士の加算
	Vector4 operator +(const Vector4& _other) const;
	// Vec4同士の減算
	Vector4 operator -(const Vector4& _other) const;
	// スカラー倍
	Vector4 operator *(const float _value) const;
	// スカラーによる除算(0割りをすると0ベクトルを返すようにしています)
	Vector4 operator /(const float _value) const;
	// Vec4同士の加算代入
	Vector4& operator +=(const Vector4& _other);
	// Vec4同士の減算代入
	Vector4& operator -=(const Vector4& _other);
	// スカラーとベクトルの乗算代入
	Vector4& operator *=(const float _value);
	// スカラーによる除算代入(0割りをすると0ベクトルを返すようにしています)
	Vector4& operator /=(const float _value);
	// ベクトルとスカラーの逆乗算
	friend Vector4 operator *(const float _value, const Vector4& _other)
	{
		return _other * _value;
	}


	// 以下比較演算子

	// 等価
	bool operator ==(const Vector4& _other) const;
	// 非等価
	bool operator !=(const Vector4& _other) const;

	float x; // x座標
	float y; // y座標
	float z; // z座標
	float w; // w座標
};