#pragma once
#include "../Matrix/Mat4x4.h"
#include "../Vector/Vector3.h"
#include "../Vector/Vector4.h"

// 四元数を提供するクラス
struct alignas(16) Quaternion
{
public:
	//　コンストラクタ群
	Quaternion() : x{ 0.0f }, y{ 0.0f }, z{ 0.0f }, w{1.0f}{} // デフォルト(0初期化)
	Quaternion(float _x, float _y, float _z, float _w); // 全てfloatで受け取る
	Quaternion(const Vector3& _vec, float _w); // 実部をfloatで受け取りそれ以外をVector3型で受け取る
	Quaternion(const Vector4& _vec); // Vector4型で受け取る

	~Quaternion() = default; // デフォルトデストラクタ

	// 回転なし
	static const Quaternion Identity;

	// 軸と角度から新しい四元数を作成する
	static Quaternion FromAxisAngle(const Vector3& _axis, float _radians);

	// オイラー角から四元数を生成する
	static Quaternion FromEuler(float _pitch, float _yaw, float _roll);

	// 補完
	static Quaternion Slerp(const Quaternion& _from, const Quaternion& _to, float _t);

	// 正規化されたコピーを返す
	static Quaternion Normalized(Quaternion _quaternion);

	// 内積
	static float Dot(const Quaternion& _a, const Quaternion& _b);

	// 四元数を回転行列に変換
	Mat4x4 ToMat4x4() const;

	// ベクトルを回転させる
	Vector3 RotateVector(const Vector3& _v) const;

	// 長さを出す
	float Length() const;

	// 長さの二乗を返す
	float LengthSquared() const;

	// 自身を正規化する
	void Normalize();

	// 四元数合成
	Quaternion operator *(const Quaternion& _other) const;

	// 比較

	bool operator ==(const Quaternion& _other) const;
	bool operator !=(const Quaternion& _other) const;

	// データメンバ
	float x{ 0.0f };
	float y{ 0.0f };
	float z{ 0.0f };
	float w{ 0.0f };

};