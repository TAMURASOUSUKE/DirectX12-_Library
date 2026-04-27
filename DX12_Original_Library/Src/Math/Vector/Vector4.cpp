#include <cmath>
#include <algorithm>
#include "../MathConstant.h"
#include "Vector4.h"


const Vector4 Vector4::Zero{ Vector4(0.0f, 0.0f, 0.0f, 0.0f) };
const Vector4 Vector4::One{ Vector4(1.0f, 1.0f, 1.0f, 1.0f) };
const Vector4 Vector4::Up{ Vector4(0.0f, 1.0f, 0.0f, 0.0f) };
const Vector4 Vector4::Down{ Vector4(0.0f, -1.0f, 0.0f, 0.0f) };
const Vector4 Vector4::Right{ Vector4(1.0f, 0.0f, 0.0f, 0.0f) };
const Vector4 Vector4::Left{ Vector4(-1.0f, 0.0f, 0.0f, 0.0f) };
const Vector4 Vector4::Forward{ Vector4(0.0f, 0.0f, 1.0f, 0.0f) };
const Vector4 Vector4::Back{ Vector4(0.0f, 0.0f, -1.0f, 0.0f) };

// 長さ
float Vector4::Length() const
{
	return std::sqrtf(x * x + y * y + z * z + w * w);
}

// 長さの二乗
float Vector4::LengthSquared() const
{
	return x * x + y * y + z * z + w * w;
}

// 正規化
void Vector4::Normalize()
{
	float len{ Length() }; // ベクトルの長さを取得

	if (len > 0.0f) // 0除防止
	{
		x /= len;
		y /= len;
		z /= len;
		w /= len;
	}
}

// Vector3への変換関数(wを捨てる)
Vector3 Vector4::ToVec3() const
{
	return Vector3{x, y , z};
}

// 内積
float Vector4::Dot(const Vector4& _vec01, const Vector4& _vec02)
{
	return _vec01.x * _vec02.x + _vec01.y * _vec02.y + _vec01.z * _vec02.z + _vec01.w * _vec02.w;
}

// 二点間ベクトルの距離
float Vector4::Distance(const Vector4& _startPos, const Vector4& _endPos)
{
	Vector4 distVec{ FromTo(_startPos, _endPos) }; // 二点間のベクトル
	return distVec.Length();
}

// 二点間ベクトルの距離の二乗
float Vector4::DistanceSquared(const Vector4& _startPos, const Vector4& _endPos)
{
	Vector4 distVec{ FromTo(_startPos, _endPos) }; // 二点間のベクトル
	return distVec.LengthSquared();
}

// 正規化されたベクトルを返す
Vector4 Vector4::Normalized(Vector4 _vec)
{
	_vec.Normalize();
	return _vec;
}

// 二点間のベクトル
Vector4 Vector4::FromTo(const Vector4& _startPos, const Vector4& _endPos)
{
	Vector4 distVec{ _endPos.x - _startPos.x, _endPos.y - _startPos.y, _endPos.z - _startPos.z, _endPos.w - _startPos.w }; // 二点間のベクトル
	return distVec;
}

// 線形補完
Vector4 Vector4::Lerp(const Vector4& _startPos, const Vector4& _endPos, float _completionValue)
{
	// 0-1の範囲に収める
	_completionValue = std::clamp(_completionValue, 0.0f, 1.0f);

	Vector4 distVec{ FromTo(_startPos, _endPos) }; // 二点間のベクトル

	return distVec * _completionValue + _startPos;
}

// Vector3を方向ベクトルとしてVector4に変換する(w = 0)
Vector4 Vector4::FromDirection(const Vector3& _dir)
{
	return Vector4{_dir, 0.0f};
}

// Vector3を位置座標としてVector4に変換する(w = 1)
Vector4 Vector4::FromPosition(const Vector3& _pos)
{
	return Vector4{_pos, 1.0f};
}

// 逆ベクトル
Vector4 Vector4::operator -() const
{
	return Vector4{ -x, -y, -z, -w };
}

// 各演算子
// Vec4同士の加算
Vector4 Vector4::operator +(const Vector4& _other) const
{
	return Vector4{ x + _other.x, y + _other.y, z + _other.z, w + _other.w };
}

// Vec4同士の減算
Vector4 Vector4::operator -(const Vector4& _other) const
{
	return Vector4{ x - _other.x, y - _other.y, z - _other.z, w - _other.w };
}

// スカラー倍
Vector4 Vector4::operator *(const float _value) const
{
	return Vector4{ x * _value, y * _value, z * _value, w * _value };
}

// スカラーによる除算(0割りをすると0ベクトルを返すようにしています)
Vector4 Vector4::operator /(const float _value) const
{
	if (_value == 0.0f)
	{
		return Zero;
	}
	else
	{
		return Vector4{ x / _value, y / _value, z / _value, w / _value };
	}
}

// Vec4同士の加算代入
Vector4& Vector4::operator +=(const Vector4& _other)
{
	x += _other.x;
	y += _other.y;
	z += _other.z;
	w += _other.w;
	return *this;
}

// Vec4同士の減算代入
Vector4& Vector4::operator -=(const Vector4& _other)
{
	x -= _other.x;
	y -= _other.y;
	z -= _other.z;
	w -= _other.w;
	return *this;
}

// スカラーとベクトルの乗算代入
Vector4& Vector4::operator *=(const float _value)
{
	x *= _value;
	y *= _value;
	z *= _value;
	w *= _value;
	return *this;
}

// スカラーによる除算代入(0割りをすると0ベクトルを返すようにしています)
Vector4& Vector4::operator /=(const float _value)
{
	if (_value == 0.0f)
	{
		x = 0.0f;
		y = 0.0f;
		z = 0.0f;
		w = 0.0f;
	}
	else
	{
		x /= _value;
		y /= _value;
		z /= _value;
		w /= _value;
	}

	return *this;
}

// 以下比較演算子

// 等価
bool Vector4::operator ==(const Vector4& _other) const
{
	// 差がEPSILONより小さいなら同じとみなす
	return std::abs(x - _other.x) < Math::EPSILON && std::abs(y - _other.y) < Math::EPSILON && std::abs(z - _other.z) < Math::EPSILON && std::abs(w - _other.w) < Math::EPSILON;
}

// 非等価
bool Vector4::operator !=(const Vector4& _other) const
{
	return !(*this == _other);
}