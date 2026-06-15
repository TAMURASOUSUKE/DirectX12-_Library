#include <cmath>
#include <algorithm>
#include "../../Debug/DebugLogs.h"
#include "../MathConstant.h"
#include "Vector2.h"


const Vector2 Vector2::Zero{ Vector2(0.0f, 0.0f) };
const Vector2 Vector2::One{ Vector2(1.0f, 1.0f) };
const Vector2 Vector2::Up{ Vector2(0.0f, 1.0f) };
const Vector2 Vector2::Down{ Vector2(0.0f, -1.0f) };
const Vector2 Vector2::Right{ Vector2(1.0f, 0.0f) };
const Vector2 Vector2::Left{ Vector2(-1.0f, 0.0f) };

// 長さ
float Vector2::Length() const
{
	return std::sqrtf(x * x + y * y);
}

// 長さの二乗
float Vector2::LengthSquared() const
{
	return x * x + y * y;
}

// 正規化
void Vector2::Normalize()
{
	float len{ Length() }; // ベクトルの長さを取得

	if (len > 0.0f) // 0除防止
	{
		x /= len;
		y /= len;
	}
}

// 内積
float Vector2::Dot(const Vector2& _vec01, const Vector2& _vec02)
{
	return _vec01.x * _vec02.x + _vec01.y * _vec02.y;
}

// 外積
float Vector2::Cross(const Vector2& _vec01, const Vector2& _vec02)
{
	return _vec01.x * _vec02.y - _vec01.y * _vec02.x;
}

// 二点間ベクトルの距離
float Vector2::Distance(const Vector2& _startPos, const Vector2& _endPos)
{
	Vector2 distVec{ FromTo(_startPos, _endPos) }; // 二点間のベクトル
	return distVec.Length();
}

// 二点間ベクトルの距離の二乗
float Vector2::DistanceSquared(const Vector2& _startPos, const Vector2& _endPos)
{
	Vector2 distVec{ FromTo(_startPos, _endPos) }; // 二点間のベクトル
	return distVec.LengthSquared();
}

// 2つのベクトルの角度
float Vector2::RadAngle(const Vector2& _from, const Vector2& _to)
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

// 2つのベクトルの符号付角度
float Vector2::SignedRadAngle(const Vector2& _from, const Vector2& _to)
{
	float angle{ RadAngle(_from, _to) }; // 二つのベクトル間の角度

	// 外積の結果から符号を判定
	float cross{ Cross(_from, _to) };

	// 外積の符号によって角度を反転させるか決める
	return (cross < 0.0f) ? -angle : angle;
}

// 2つのベクトルの角度
float Vector2::DegAngle(const Vector2& _from, const Vector2& _to)
{
	 return RadAngle(_from, _to) * Math::RAD_TO_DEG; // 度数法変換
}

// 2つのベクトルの符号付角度
float Vector2::SignedDegAngle(const Vector2& _from, const Vector2& _to)
{
	return SignedRadAngle(_from, _to) * Math::RAD_TO_DEG; // 度数法変換
}

// 正規化されたベクトルを返す
Vector2 Vector2::Normalized(Vector2 _vec)
{
	_vec.Normalize();
	return _vec;
}

// 二点間のベクトル
Vector2 Vector2::FromTo(const Vector2& _startPos, const Vector2& _endPos)
{
	Vector2 distVec{ _endPos.x - _startPos.x, _endPos.y - _startPos.y }; // 二点間のベクトル
	return distVec;
}

// 線形補完
Vector2 Vector2::Lerp(const Vector2& _startPos, const Vector2& _endPos, float _completionValue)
{
	// 0-1の範囲に収める
	_completionValue = std::clamp(_completionValue, 0.0f, 1.0f);

	Vector2 distVec{ FromTo(_startPos, _endPos) }; // 二点間のベクトル

	return distVec * _completionValue + _startPos;
}



// 逆ベクトル
Vector2 Vector2::operator -() const
{
	return Vector2{ -x, -y };
}

// 各演算子
// Vec2同士の加算
Vector2 Vector2::operator +(const Vector2& _other) const
{
	return Vector2{ x + _other.x, y + _other.y };
}

// Vec2同士の減算
Vector2 Vector2::operator -(const Vector2& _other) const
{
	return Vector2{ x - _other.x, y - _other.y };
}

// スカラー倍
Vector2 Vector2::operator *(const float _value) const
{
	return Vector2{ x * _value, y * _value };
}

// スカラーによる除算(0割りをすると0ベクトルを返すようにしています)
Vector2 Vector2::operator /(const float _value) const
{
	if (_value == 0.0f)
	{
		DEBUG_LOG_WARNING("0除算しようとしました\n");
		return Zero;
	}
	else
	{
		return Vector2{ x / _value, y / _value };
	}
}

// Vec2同士の加算代入
Vector2& Vector2::operator +=(const Vector2& _other)
{
	x += _other.x;
	y += _other.y;
	return *this;
}

// Vec2同士の減算代入
Vector2& Vector2::operator -=(const Vector2& _other)
{
	x -= _other.x;
	y -= _other.y;
	return *this;
}

// スカラーとベクトルの乗算代入
Vector2& Vector2::operator *=(const float _value)
{
	x *= _value;
	y *= _value;
	return *this;
}

// スカラーによる除算代入(0割りをすると0ベクトルを返すようにしています)
Vector2& Vector2::operator /=(const float _value)
{
	if (_value == 0.0f)
	{
		DEBUG_LOG_WARNING("0除算しようとしました\n");
		x = 0.0f;
		y = 0.0f;
	}
	else
	{
		x /= _value;
		y /= _value;
	}

	return *this;
}

// 以下比較演算子

// 等価
bool Vector2::operator ==(const Vector2& _other) const
{
	// 差がEPSILONより小さいなら同じとみなす
	return std::abs(x - _other.x) < Math::EPSILON && std::abs(y - _other.y) < Math::EPSILON;
}

// 非等価
bool Vector2::operator !=(const Vector2& _other) const
{
	return !(*this == _other);
}