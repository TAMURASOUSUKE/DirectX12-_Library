#include <cmath>
#include <algorithm>
#include "../MathConstant.h"
#include "Quaternion.h"

// 定数
const Quaternion Quaternion::Identity{ 0.0f, 0.0f, 0.0f, 1.0f };

// 軸と角度から生成する
Quaternion Quaternion::FromAxisAngle(const Vector3& _axis, float _radians)
{
	Vector3 axis{ Vector3::Normalized(_axis) }; // 軸の正規化

	float halfAngle{ _radians / 2.0f }; // 半角

	// sinとcosのキャッシュ
	float s{ std::sinf(halfAngle) };
	float c{ std::cosf(halfAngle) };

	// 実部にはcos(θ/2)虚部にはsin(θ/2)
	Quaternion result
	{
		axis.x * s,
		axis.y * s,
		axis.z * s,
		c
	};
	result.Normalize();
	return result;
}

// オイラー角から生成する
Quaternion Quaternion::FromEuler(const float _pitch, const float _yaw, const float _roll)
{
	Quaternion qx{ FromAxisAngle(Vector3::Right, _pitch) };
	Quaternion qy{ FromAxisAngle(Vector3::Up, _yaw) };
	Quaternion qz{ FromAxisAngle(Vector3::Forward, _roll) };

	// Unityと同じ基準でYXZ順序を採用。
	// pitch±90°でジンバルロックがかかるが実行中に向くのは稀かつそもそも内部でFromEuler関数を使うことが稀なため
	// この方法を採用
	Quaternion result{ qy * qx * qz };
	result.Normalize();
	return result;
}

Quaternion Quaternion::FromEuler(const Vector3& rotation)
{
	return FromEuler(rotation.x, rotation.y, rotation.z);
}

// 補完
Quaternion Quaternion::Slerp(const Quaternion& _from, const Quaternion& _to, float _t)
{
	_t = std::clamp(_t, 0.0f, 1.0f);

	Quaternion from{ Normalized(_from) };
	Quaternion to{ Normalized(_to) };

	float dot{ Dot(from, to) };

	if (dot < 0.0f)
	{
		to.x = -to.x;
		to.y = -to.y;
		to.z = -to.z;
		to.w = -to.w;
		dot = -dot;
	}

	constexpr float DOT_THRESHOLD = 0.9995f;
	if (dot > DOT_THRESHOLD)
	{
		Quaternion result
		{
			from.x + (to.x - from.x) * _t,
			from.y + (to.y - from.y) * _t,
			from.z + (to.z - from.z) * _t,
			from.w + (to.w - from.w) * _t
		};
		result.Normalize();
		return result;
	}

	float theta0{ std::acosf(std::clamp(dot, -1.0f, 1.0f)) };
	float theta{ theta0 * _t };

	float sinTheta{ std::sinf(theta) };
	float sinTheta0{ std::sinf(theta0) };

	float s0{ std::cosf(theta) - dot * sinTheta / sinTheta0 };
	float s1{ sinTheta / sinTheta0 };

	Quaternion result
	{
		from.x * s0 + to.x * s1,
		from.y * s0 + to.y * s1,
		from.z * s0 + to.z * s1,
		from.w * s0 + to.w * s1
	};

	result.Normalize();
	return result;
}

// 正規化
Quaternion Quaternion::Normalized(Quaternion _quaternion)
{
	_quaternion.Normalize();
	return _quaternion;
}

// 内積
float Quaternion::Dot(const Quaternion& _a, const Quaternion& _b)
{
	return _a.x * _b.x + _a.y * _b.y + _a.z * _b.z + _a.w * _b.w;
}

// 長さ
float Quaternion::Length() const
{
	return std::sqrtf(x * x + y * y + z * z + w * w);
}

// 4x4行列の変換
Mat4x4 Quaternion::ToMat4x4() const
{
	Quaternion q{ Normalized((*this))};
	float xx{ q.x * q.x };
	float yy{ q.y * q.y };
	float zz{ q.z * q.z };
	float xy{ q.x * q.y };
	float xz{ q.x * q.z };
	float yz{ q.y * q.z };
	float wx{ q.w * q.x };
	float wy{ q.w * q.y };
	float wz{ q.w * q.z };

	return Mat4x4
	{
		Vector4{ 1.0f - 2.0f * (yy + zz), 2.0f * (xy + wz),         2.0f * (xz - wy),         0.0f },
		Vector4{ 2.0f * (xy - wz),         1.0f - 2.0f * (xx + zz), 2.0f * (yz + wx),         0.0f },
		Vector4{ 2.0f * (xz + wy),         2.0f * (yz - wx),         1.0f - 2.0f * (xx + yy), 0.0f },
		Vector4{ 0.0f,                     0.0f,                     0.0f,                     1.0f }
	};
}

// ベクトルの回転
Vector3 Quaternion::RotateVector(const Vector3& _vec) const
{
	Quaternion q{ Normalized((*this)) };
	Quaternion vq{ _vec, 0.0f };
	Quaternion inv{ -q.x, -q.y, -q.z, q.w };

	Quaternion result{ inv * vq * q };
	return Vector3{ result.x, result.y, result.z };
}

// 長さの二乗
float Quaternion::LengthSquared() const
{
	return x * x + y * y + z * z + w * w;
}

// 自身を正規化する
void Quaternion::Normalize()
{
	float length{ Length() };

	if (length > Math::EPSILON)
	{
		x /= length;
		y /= length;
		z /= length;
		w /= length;
	}
}


// operator
Quaternion Quaternion::operator *(const Quaternion& _other) const
{
	return Quaternion
	{
		w * _other.x + x * _other.w + y * _other.z - z * _other.y,
		w * _other.y - x * _other.z + y * _other.w + z * _other.x,
		w * _other.z + x * _other.y - y * _other.x + z * _other.w,
		w * _other.w - x * _other.x - y * _other.y - z * _other.z
	};
}

// 等価
bool Quaternion::operator ==(const Quaternion& _other) const
{
	return
	{
		std::abs(x - _other.x) < Math::EPSILON && std::abs(y - _other.y) < Math::EPSILON && std::abs(z - _other.z) < Math::EPSILON && std::abs(w - _other.w) < Math::EPSILON
	};
}

// 非等価
bool Quaternion::operator !=(const Quaternion& _other) const
{
	return !(*this == _other);
}
