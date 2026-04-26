#pragma once

/// <summary>
/// 16バイト境界で配置される三次元座標や三次元ベクトルについての機能をまとめた構造体です。
/// 内積、外積、長さ、長さの二乗、 線形補完、二点間ベクトル、二点間の距離、二点間の距離の二乗、
/// 各演算、等価、非等価、ベクトルの長さの比較を比較演算子で行う、四方向、0,1代入定数
/// などの機能を実装しています
/// </summary>
struct alignas(16) Vector3
{
	// デフォルトコンストラクタでは0ベクトルで初期化する
	Vector3() : x{ 0.0f }, y{ 0.0f }, z{ 0.0f }, pad{ 0.0f } {}
	// 3次元座標を引数で設定するコンストラクタ
	Vector3(float _x, float _y, float _z) : x{ _x }, y{ _y }, z{ _z }, pad{ 0.0f } {}
	// デフォルトデストラクタ
	~Vector3() = default;

	// 各定数
	static const Vector3 Zero;
	static const Vector3 One;
	static const Vector3 Up;
	static const Vector3 Down;
	static const Vector3 Right;
	static const Vector3 Left;
	static const Vector3 Forward;
	static const Vector3 Back;

	// ベクトルの長さをfloatで返す
	float Length() const;

	// ベクトルの長さの2乗を返す(平方根を使わないので若干軽いです)
	float LengthSquared() const;

	// ベクトルを正規化する(0ベクトルはそのまま)
	void Normalize();

	// 内積を行う
	static float Dot(const Vector3& _vec01, const Vector3& _vec02);

	// 二つのベクトルの距離
	static float Distance(const Vector3& _startPos, const Vector3& _endPos);

	// 二つのベクトルの距離の二乗
	static float DistanceSquared(const Vector3& _startPos, const Vector3& _endPos);

	// 二つのベクトル間の角度(ラジアンで返します)
	static float RadAngle(const Vector3& _from, const Vector3& _to);

	// 二つのベクトル間の角度を符号付で返す(ラジアンで返します。第三引数には角度の基準となる軸を入れてください。)
	static float SignedRadAngle(const Vector3& _from, const Vector3& _to, const Vector3& _axis);

	// 二つのベクトル間の角度(デグリーで返します)
	static float DegAngle(const Vector3& _from, const Vector3& _to);

	// 二つのベクトル間の角度を符号付で返す(デグリーで返します。第三引数には角度の基準となる軸を入れてください。)
	static float SignedDegAngle(const Vector3& _from, const Vector3& _to, const Vector3& _axis);

	// 外積を行う
	static Vector3 Cross(const Vector3& _vec01, const Vector3& _vec02);

	// 正規化されたベクトルを返す(0ベクトルならそのまま)
	static Vector3 Normalized(Vector3 _vec);

	// 二点間のベクトル
	static Vector3 FromTo(const Vector3& _startPos, const Vector3& _endPos);

	// 二点間の二次元座標を第三引数の値で補完する(第三引数は0から1の間の数で渡してください)
	static Vector3 Lerp(const Vector3& _startPos, const Vector3& _endPos, float _completionValue);

	// 逆ベクトル
	Vector3 operator -() const;

	// 以下演算

	// Vec3同士の加算
	Vector3 operator +(const Vector3& _other) const;
	// Vec3同士の減算
	Vector3 operator -(const Vector3& _other) const;
	// スカラー倍
	Vector3 operator *(const float _value) const;
	// スカラーによる除算(0割りをすると0ベクトルを返すようにしています)
	Vector3 operator /(const float _value) const;
	// Vec3同士の加算代入
	Vector3& operator +=(const Vector3& _other);
	// Vec3同士の減算代入
	Vector3& operator -=(const Vector3& _other);
	// スカラーとベクトルの乗算代入
	Vector3& operator *=(const float _value);
	// スカラーによる除算代入(0割りをすると0ベクトルを返すようにしています)
	Vector3& operator /=(const float _value);

	// ベクトルとスカラーの逆乗算
	friend Vector3 operator *(const float _value, const Vector3& _other)
	{
		return _other * _value;
	}


	// 以下比較演算子

	// 等価
	bool operator ==(const Vector3& _other) const;
	// 非等価
	bool operator !=(const Vector3& _other) const;

	float x; // x座標
	float y; // y座標
	float z; // z座標
	float pad; // パディング
};