#include <cmath>
#include "Vector2Int.h"


const Vector2Int Vector2Int::Zero{ Vector2Int(0, 0) };
const Vector2Int Vector2Int::One{ Vector2Int(1, 1) };
const Vector2Int Vector2Int::Up{ Vector2Int(0, 1) };
const Vector2Int Vector2Int::Down{ Vector2Int(0, -1) };
const Vector2Int Vector2Int::Right{ Vector2Int(1, 0) };
const Vector2Int Vector2Int::Left{ Vector2Int(-1, 0) };

// 長さ
float Vector2Int::Length() const
{
	return std::sqrt(static_cast<float>(x * x + y * y));
}

// 長さの二乗
int Vector2Int::LengthSquared() const
{
	return x * x + y * y;
}

// 内積
int Vector2Int::Dot(const Vector2Int& _vec01, const Vector2Int& _vec02)
{
	return _vec01.x * _vec02.x + _vec01.y * _vec02.y;
}

// 外積
int Vector2Int::Cross(const Vector2Int& _vec01, const Vector2Int& _vec02)
{
	return _vec01.x * _vec02.y - _vec01.y * _vec02.x;
}

// 二点間ベクトルの距離
float Vector2Int::Distance(const Vector2Int& _startPos, const Vector2Int& _endPos)
{
	Vector2Int distVec{ FromTo(_startPos, _endPos) }; // 二点間のベクトル
	return distVec.Length();
}

// 二点間ベクトルの距離の二乗
int Vector2Int::DistanceSquared(const Vector2Int& _startPos, const Vector2Int& _endPos)
{
	Vector2Int distVec{ FromTo(_startPos, _endPos) }; // 二点間のベクトル
	return distVec.LengthSquared();
}

// 二点間のベクトル
Vector2Int Vector2Int::FromTo(const Vector2Int& _startPos, const Vector2Int& _endPos)
{
	Vector2Int distVec{ _endPos.x - _startPos.x, _endPos.y - _startPos.y }; // 二点間のベクトル
	return distVec;
}


// 逆ベクトル
Vector2Int Vector2Int::operator -() const
{
	return Vector2Int{ -x, -y };
}

// 各演算子
// Vec2同士の加算
Vector2Int Vector2Int::operator +(const Vector2Int& _other) const
{
	return Vector2Int{ x + _other.x, y + _other.y };
}

// Vec2同士の減算
Vector2Int Vector2Int::operator -(const Vector2Int& _other) const
{
	return Vector2Int{ x - _other.x, y - _other.y };
}

// スカラー倍
Vector2Int Vector2Int::operator *(const int _value) const
{
	return Vector2Int{ x * _value, y * _value };
}

// スカラーによる除算(0割りをすると0ベクトルを返すようにしています。整数除算なので小数部分は切り捨てられます)
Vector2Int Vector2Int::operator /(const int _value) const
{
	if (_value == 0)
	{
		return Zero;
	}
	else
	{
		return Vector2Int{ x / _value, y / _value};
	}
}

// Vec2同士の加算代入
Vector2Int& Vector2Int::operator +=(const Vector2Int& _other)
{
	x += _other.x;
	y += _other.y;
	return *this;
}

// Vec2同士の減算代入
Vector2Int& Vector2Int::operator -=(const Vector2Int& _other)
{
	x -= _other.x;
	y -= _other.y;
	return *this;
}

// スカラーとベクトルの乗算代入
Vector2Int& Vector2Int::operator *=(const int _value)
{
	x *= _value;
	y *= _value;
	return *this;
}

// スカラーによる除算代入(0割りをすると0ベクトルを返すようにしています。整数除算なので小数点は失われます)
Vector2Int& Vector2Int::operator /=(const int _value)
{
	if (_value == 0)
	{
		x = 0;
		y = 0;
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
bool Vector2Int::operator ==(const Vector2Int& _other) const
{
	return x == _other.x && y == _other.y;
}

// 非等価
bool Vector2Int::operator !=(const Vector2Int& _other) const
{
	return x != _other.x || y != _other.y;
}