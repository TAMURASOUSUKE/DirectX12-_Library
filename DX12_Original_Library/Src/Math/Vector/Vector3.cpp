#include <cmath>
#include <algorithm>
#include "../MathConstant.h"
#include "Vector3.h"


const Vector3 Vector3::Zero{ Vector3(0.0f, 0.0f, 0.0f) };
const Vector3 Vector3::One{ Vector3(1.0f, 1.0f, 1.0f) };
const Vector3 Vector3::Up{ Vector3(0.0f, 1.0f, 0.0f) };
const Vector3 Vector3::Down{ Vector3(0.0f, -1.0f, 0.0f) };
const Vector3 Vector3::Right{ Vector3(1.0f, 0.0f, 0.0f) };
const Vector3 Vector3::Left{ Vector3(-1.0f, 0.0f, 0.0f) };
const Vector3 Vector3::Forward{ Vector3(0.0f, 0.0f, 1.0f) };
const Vector3 Vector3::Back{ Vector3(0.0f, 0.0f, -1.0f) };

// 長さ
float Vector3::Length() const
{
	return std::sqrtf(x * x + y * y + z * z);
}

// 長さの二乗
float Vector3::LengthSquared() const
{
	return x * x + y * y + z * z;
}

// 正規化
void Vector3::Normalize()
{
	float len{ Length() }; // ベクトルの長さを取得

	if (len > 0.0f) // 0除防止
	{
		x /= len;
		y /= len;
		z /= len;
	}
}

// 内積
float Vector3::Dot(const Vector3& _vec01, const Vector3& _vec02)
{
	return _vec01.x * _vec02.x + _vec01.y * _vec02.y + _vec01.z * _vec02.z;
}

// 二点間ベクトルの距離
float Vector3::Distance(const Vector3& _startPos, const Vector3& _endPos)
{
	Vector3 distVec{ FromTo(_startPos, _endPos) }; // 二点間のベクトル
	return distVec.Length();
}

// 二点間ベクトルの距離の二乗
float Vector3::DistanceSquared(const Vector3& _startPos, const Vector3& _endPos)
{
	Vector3 distVec{ FromTo(_startPos, _endPos) }; // 二点間のベクトル
	return distVec.LengthSquared();
}

// 2つのベクトルの角度
float Vector3::RadAngle(const Vector3& _from, const Vector3& _to)
{
	// それぞれの長さ
	float fromLen{ _from.Length() };
	float toLen{ _to.Length() };

	// どちらかのベクトルがEPSILON値より小さければ0とみなし0.0fを返す
	if (fromLen < Math::EPSILON || toLen < Math::EPSILON)
	{
		return 0.0f;
	}

	float cosTheta{ (Dot(_from, _to) / (fromLen * toLen)) };
	return std::acosf(std::clamp(cosTheta, -1.0f, 1.0f)); // 誤差が出ないよう範囲を丸める
}

// 二つのベクトル間の角度を符号付で返す
float Vector3::SignedRadAngle(const Vector3& _from, const Vector3& _to, const Vector3& _axis)
{
	float angle{ RadAngle(_from, _to) }; // 角度

	Vector3 cross{ Cross(_from, _to) };// 外積ベクトル

	float dot{ Dot(cross, _axis) }; // 基準軸と外積のベクトルの内積が正か負かで判定

	return (dot < 0.0f) ? -angle : angle;
}

// 2つのベクトルの角度
float Vector3::DegAngle(const Vector3& _from, const Vector3& _to)
{
	return RadAngle(_from, _to) * Math::RAD_TO_DEG; // 度数法変換
}

// 2つのベクトルの角度
float Vector3::SignedDegAngle(const Vector3& _from, const Vector3& _to, const Vector3& _axis)
{
	return SignedRadAngle(_from, _to, _axis) * Math::RAD_TO_DEG; // 度数法変換
}

// 外積
Vector3 Vector3::Cross(const Vector3& _vec01, const Vector3& _vec02)
{
	return Vector3
	{
		_vec01.y * _vec02.z - _vec01.z * _vec02.y,
		_vec01.z * _vec02.x - _vec01.x * _vec02.z,
		_vec01.x * _vec02.y - _vec01.y * _vec02.x
	};
}

// 正規化されたベクトルを返す
Vector3 Vector3::Normalized(Vector3 _vec)
{
	_vec.Normalize();
	return _vec;
}

// 二点間のベクトル
Vector3 Vector3::FromTo(const Vector3& _startPos, const Vector3& _endPos)
{
	Vector3 distVec{ _endPos.x - _startPos.x, _endPos.y - _startPos.y, _endPos.z - _startPos.z }; // 二点間のベクトル
	return distVec;
}

// 線形補完
Vector3 Vector3::Lerp(const Vector3& _startPos, const Vector3& _endPos, float _completionValue)
{
	// 0-1の範囲に収める
	_completionValue = std::clamp(_completionValue, 0.0f, 1.0f);

	Vector3 distVec{ FromTo(_startPos, _endPos)}; // 二点間のベクトル

	return distVec * _completionValue + _startPos;
}

// 逆ベクトル
Vector3 Vector3::operator -() const
{
	return Vector3{ -x, -y, -z };
}

// 各演算子
// Vec3同士の加算
Vector3 Vector3::operator +(const Vector3& _other) const
{
	return Vector3{ x + _other.x, y + _other.y, z + _other.z };
}

// Vec3同士の減算
Vector3 Vector3::operator -(const Vector3& _other) const
{
	return Vector3{ x - _other.x, y - _other.y, z - _other.z };
}

// スカラー倍
Vector3 Vector3::operator *(const float _value) const
{
	return Vector3{ x * _value, y * _value, z * _value };
}

// スカラーによる除算(0割りをすると0ベクトルを返すようにしています)
Vector3 Vector3::operator /(const float _value) const
{
	if (_value == 0.0f)
	{
		return Zero;
	}
	else
	{
		return Vector3{ x / _value, y / _value, z / _value };
	}
}

// Vec3同士の加算代入
Vector3& Vector3::operator +=(const Vector3& _other)
{
	x += _other.x;
	y += _other.y;
	z += _other.z;
	return *this;
}

// Vec3同士の減算代入
Vector3& Vector3::operator -=(const Vector3& _other)
{
	x -= _other.x;
	y -= _other.y;
	z -= _other.z;
	return *this;
}

// スカラーとベクトルの乗算代入
Vector3& Vector3::operator *=(const float _value)
{
	x *= _value;
	y *= _value;
	z *= _value;
	return *this;
}

// スカラーによる除算代入(0割りをすると0ベクトルを返すようにしています)
Vector3& Vector3::operator /=(const float _value)
{
	if (_value == 0.0f)
	{
		x = 0.0f;
		y = 0.0f;
		z = 0.0f;
	}
	else
	{
		x /= _value;
		y /= _value;
		z /= _value;
	}

	return *this;
}

// 以下比較演算子



// 等価
bool Vector3::operator ==(const Vector3& _other) const
{
	// 差がEPSILONより小さいなら同じとみなす
	return std::abs(x - _other.x) < Math::EPSILON && std::abs(y - _other.y) < Math::EPSILON && std::abs(z - _other.z) < Math::EPSILON;
}

// 非等価
bool Vector3::operator !=(const Vector3& _other) const
{
	return !(*this == _other);
}