#include <cmath>
#include "Vector3Int.h"


const Vector3Int Vector3Int::Zero{ Vector3Int(0, 0, 0) };
const Vector3Int Vector3Int::One{ Vector3Int(1, 1, 1) };
const Vector3Int Vector3Int::Up{ Vector3Int(0, 1, 0) };
const Vector3Int Vector3Int::Down{ Vector3Int(0, -1, 0) };
const Vector3Int Vector3Int::Right{ Vector3Int(1, 0, 0) };
const Vector3Int Vector3Int::Left{ Vector3Int(-1, 0, 0) };
const Vector3Int Vector3Int::Forward{ Vector3Int(0, 0, 1) };
const Vector3Int Vector3Int::Back{ Vector3Int(0, 0, -1) };

// 長さ
float Vector3Int::Length() const
{
	return std::sqrt(static_cast<float>(x * x + y * y + z * z));
}

// 長さの二乗
int Vector3Int::LengthSquared() const
{
	return x * x + y * y + z * z;
}


// 内積
int Vector3Int::Dot(const Vector3Int& _vec01, const Vector3Int& _vec02)
{
	return _vec01.x * _vec02.x + _vec01.y * _vec02.y + _vec01.z * _vec02.z;
}

// 二点間ベクトルの距離
float Vector3Int::Distance(const Vector3Int& _startPos, const Vector3Int& _endPos)
{
	Vector3Int distVec{ FromTo(_startPos, _endPos) }; // 二点間のベクトル
	return distVec.Length();
}

// 二点間ベクトルの距離の二乗
int Vector3Int::DistanceSquared(const Vector3Int& _startPos, const Vector3Int& _endPos)
{
	Vector3Int distVec{ FromTo(_startPos, _endPos) }; // 二点間のベクトル
	return distVec.LengthSquared();
}

// 外積
Vector3Int Vector3Int::Cross(const Vector3Int& _vec01, const Vector3Int& _vec02)
{
	return Vector3Int
	{
		_vec01.y * _vec02.z - _vec01.z * _vec02.y,
		_vec01.z * _vec02.x - _vec01.x * _vec02.z,
		_vec01.x * _vec02.y - _vec01.y * _vec02.x
	};
}

// 二点間のベクトル
Vector3Int Vector3Int::FromTo(const Vector3Int& _startPos, const Vector3Int& _endPos)
{
	Vector3Int distVec{ _endPos.x - _startPos.x, _endPos.y - _startPos.y, _endPos.z - _startPos.z }; // 二点間のベクトル
	return distVec;
}

// 逆ベクトル
Vector3Int Vector3Int::operator -() const
{
	return Vector3Int{ -x, -y, -z };
}

// 各演算子
// Vec2同士の加算
Vector3Int Vector3Int::operator +(const Vector3Int& _other) const
{
	return Vector3Int{ x + _other.x, y + _other.y, z  + _other.z };
}

// Vec2同士の減算
Vector3Int Vector3Int::operator -(const Vector3Int& _other) const
{
	return Vector3Int{ x - _other.x, y - _other.y, z - _other.z };
}

// スカラー倍
Vector3Int Vector3Int::operator *(const int _value) const
{
	return Vector3Int{ x * _value, y * _value, z * _value };
};

// スカラーによる除算(0割りをすると0ベクトルを返すようにしています。整数除算なので小数部分は切り捨てられます)
Vector3Int Vector3Int::operator /(const int _value) const
{
	if (_value == 0)
	{
		return Zero;
	}
	else
	{
		return Vector3Int{ x / _value, y / _value, z / _value };
	}
}

// Vec2同士の加算代入
Vector3Int& Vector3Int::operator +=(const Vector3Int& _other)
{
	x += _other.x;
	y += _other.y;
	z += _other.z;
	return *this;
}

// Vec2同士の減算代入
Vector3Int& Vector3Int::operator -=(const Vector3Int& _other)
{
	x -= _other.x;
	y -= _other.y;
	z -= _other.z;
	return *this;
}

// スカラーとベクトルの乗算代入
Vector3Int& Vector3Int::operator *=(const int _value)
{
	x *= _value;
	y *= _value;
	z *= _value;
	return *this;
}

// スカラーによる除算代入(0割りをすると0ベクトルを返すようにしています。整数除算なので小数点は失われます)
Vector3Int& Vector3Int::operator /=(const int _value)
{
	if (_value == 0)
	{
		x = 0;
		y = 0;
		z = 0;
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
bool Vector3Int::operator ==(const Vector3Int& _other) const
{
	return x == _other.x && y == _other.y && z == _other.z;
}

// 非等価
bool Vector3Int::operator !=(const Vector3Int& _other) const
{
	return x != _other.x || y != _other.y || z != _other.z;
}