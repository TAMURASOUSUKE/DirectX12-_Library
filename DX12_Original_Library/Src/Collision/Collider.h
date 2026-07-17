#pragma once
#include "../Math/TSMath.h"

// 形状定義
struct Rect
{
	Rect() = default;
	Rect(Vector2 _position, Vector2 _size) : position{ _position }, size{ _size }{}

	// Getter類
	Vector2 GetCenter() const
	{
		return position + (size * 0.5f);
	}

	Vector2 GetHalfSize() const
	{
		return size * 0.5f;
	}

	Vector2 GetMinPos() const
	{
		return position;
	}

	Vector2 GetMaxPos() const
	{
		return position + size;
	}

	Vector2 position{Vector2::Zero}; // 左上座標
	Vector2 size{Vector2::Zero}; // サイズ
};