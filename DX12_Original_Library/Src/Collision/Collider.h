#pragma once
#include <cmath>
#include "../Math/TSMath.h"

// 形状定義
struct Rect
{
	Rect() = default;
	Rect(Vector2 _position, Vector2 _size) : position{ _position }, size{ _size }{}

	// Getter類
	Vector2 GetCenter() const { return position + (size * 0.5f); } // 中心を取得
	Vector2 GetHalfSize() const { return  size * 0.5f; } // 半分のサイズを取得
	Vector2 GetMinPos() const { return position; } // 最小座標を取得
	Vector2 GetMaxPos() const { return position + size; } // 最大座標を取得

	Vector2 position{Vector2::Zero}; // 左上座標
	Vector2 size{Vector2::Zero}; // サイズ
};

// ワールド空間の回転可能な箱
struct Box
{
	Box() = default;

	Box(Vector3 _center, Vector3 _halfSize, Quaternion _rotation = Quaternion::Identity) : center{_center}, halfSize{_halfSize}, rotation{_rotation} {}

	Vector3 GetSize() const { return center * 2.0f; } 	// Boxのサイズを返す

	// 正しい値が入っているか確認
	bool IsValid() const
	{
		return std::isfinite(center.x) && std::isfinite(center.y) && std::isfinite(center.z) &&
			std::isfinite(halfSize.x) && std::isfinite(halfSize.y) && std::isfinite(halfSize.z) &&
			std::isfinite(rotation.x) && std::isfinite(rotation.y) && std::isfinite(rotation.z) && std::isfinite(rotation.w) &&
			halfSize.x >= 0.0f && halfSize.y >= 0.0f && halfSize.z >= 0.0f && // 0は薄い壁として許可する(描画は別)
			std::abs(rotation.LengthSquared() - 1) <= Math::EPSILON; // 回転用Quaternionの長さの二乗は1。丸め誤差対策でEPSILON確認
	}

	Vector3 center{ Vector3::Zero };
	Vector3 halfSize{ Vector3::Zero };
	Quaternion rotation{ Quaternion::Identity };
};

// ワールド空間の球
struct Sphere
{
	Sphere() = default;
	
	// 中心と半径で初期化する
	Sphere(Vector3 _center, float _radius) : center{ _center }, radius{ _radius }{}

	// 座標,半径が有限値で半径が負ではないかを確認
	bool  IsValid() const { return std::isfinite(center.x) && std::isfinite(center.y) && std::isfinite(center.z) && std::isfinite(radius) && radius >= 0.0f; }

	Vector3 center{ Vector3::Zero }; // 球の中心
	float radius{ 0.0f };           // 球の半径

};
