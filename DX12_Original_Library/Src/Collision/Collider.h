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

// ワールド座標軸に沿った3D境界ボックス
struct AABB
{
	AABB() = default;

	AABB(Vector3 _minPosition, Vector3 _maxPosition) : minPosition{ _minPosition }, maxPosition{ _maxPosition } {}

	// 最小座標と最大座標の中間を返す
	Vector3 GetCenter() const
	{
		return (minPosition + maxPosition) * 0.5f;
	}

	// 最小座標から最大座標までの全長を返す
	Vector3 GetSize() const
	{
		return maxPosition - minPosition;
	}

	// 各軸の半分の長さを返す
	Vector3 GetHalfSize() const
	{
		return GetSize() * 0.5f;
	}

	// 最小座標が最大座標を追い越していないか確認する
	bool IsValid() const
	{
		return minPosition.x <= maxPosition.x && minPosition.y <= maxPosition.y && minPosition.z <= maxPosition.z;
	}

	Vector3 minPosition{ Vector3::Zero };
	Vector3 maxPosition{ Vector3::Zero };
};
