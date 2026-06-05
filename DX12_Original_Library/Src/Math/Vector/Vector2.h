#pragma once

/// <summary>
/// 二次元座標や二次元ベクトルについての機能をまとめた構造体です。
/// 内積、外積、長さ、長さの二乗、 線形補完、二点間ベクトル、二点間の距離、二点間の距離の二乗
/// 各演算、等価、非等価、ベクトルの長さの比較を比較演算子で行う、四方向、0,1代入定数
/// などの機能を実装しています
/// </summary>
struct Vector2
{
	// 引数に何も入れない場合は0ベクトルにする
	constexpr Vector2() : x{ 0 }, y{ 0 } {}
	~Vector2() = default;

	// 引数の値で二次元座標を作成する
	constexpr Vector2(float _x, float _y) : x{ _x }, y{ _y } {}

	// 各定数
	static const Vector2 Zero;
	static const Vector2 One;
	static const Vector2 Up;
	static const Vector2 Down;
	static const Vector2 Right;
	static const Vector2 Left;

	// ベクトルの長さをfloatで返す
	float Length() const;

	// ベクトルの長さの2乗を返す(平方根を使わないので若干軽いです)
	float LengthSquared() const;

	// ベクトルを正規化する(0ベクトルはそのまま)
	void Normalize();

	// 内積を行う
	static float Dot(const Vector2& _vec01, const Vector2& _vec02);

	// 外積を行う(2次元ベクトルの外積なので正なら反時計回り、負なら時計回りとなる)
	static float Cross(const Vector2& _vec01, const Vector2& _vec02);

	// 二つのベクトルの距離
	static float Distance(const Vector2& _startPos, const Vector2& _endPos);

	// 二つのベクトルの距離の二乗
	static float DistanceSquared(const Vector2& _startPos, const Vector2& _endPos);

	// 二つのベクトル間の角度(ラジアンで返します)
	static float RadAngle(const Vector2& _from, const Vector2& _to);

	// 二つのベクトル間の角度を符号付で返す(ラジアンで返します)
	static float SignedRadAngle(const Vector2& _from, const Vector2& _to);

	// 二つのベクトル間の角度(デグリーで返します)
	static float DegAngle(const Vector2& _from, const Vector2& _to);

	// 二つのベクトル間の角度を符号付で返す(デグリーで返します)
	static float SignedDegAngle(const Vector2& _from, const Vector2& _to);

	// 正規化されたベクトルを返す(0ベクトルならそのまま)
	static Vector2 Normalized(Vector2 _vec);

	// 二点間のベクトル
	static Vector2 FromTo(const Vector2& _startPos, const Vector2& _endPos);

	// 二点間の二次元座標を第三引数の値で補完する(第三引数は0から1の間の数で渡してください)
	static Vector2 Lerp(const Vector2& _startPos, const Vector2& _endPos, float _completionValue);	

	// 逆ベクトル
	Vector2 operator -() const;

	// 以下演算

	// Vec2同士の加算
	Vector2 operator +(const Vector2& _other) const;
	// Vec2同士の減算
	Vector2 operator -(const Vector2& _other) const;
	// スカラー倍
	Vector2 operator *(const float _value) const;
	// スカラーによる除算(0割りをすると0ベクトルを返すようにしています)
	Vector2 operator /(const float _value) const;
	// Vec2同士の加算代入
	Vector2& operator +=(const Vector2& _other);
	// Vec2同士の減算代入
	Vector2& operator -=(const Vector2& _other);
	// スカラーとベクトルの乗算代入
	Vector2& operator *=(const float _value);
	// スカラーによる除算代入(0割りをすると0ベクトルを返すようにしています)
	Vector2& operator /=(const float _value);

	// ベクトルとスカラーの逆乗算
	friend Vector2 operator *(const float _value, const Vector2& _other)
	{
		return _other * _value;
	}


	// 以下比較演算子

	// 等価
	bool operator ==(const Vector2& _other) const;
	// 非等価
	bool operator !=(const Vector2& _other) const;

	float x; // x座標
	float y; // y座標
};