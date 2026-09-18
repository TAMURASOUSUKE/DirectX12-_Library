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

// ワールド座標軸に沿った3D境界ボックス
struct Box
{
	Box() = default;

	Box(Vector3 _minPosition, Vector3 _maxPosition) : minPosition{ _minPosition }, maxPosition{ _maxPosition } {}

	Vector3 GetCenter() const { return (minPosition + maxPosition) * 0.5f; } // 最小座標と最大座標の中間を返す
	Vector3 GetSize() const { return maxPosition - minPosition; } 	// 最小座標から最大座標までの全長を返す
	Vector3 GetHalfSize() const { return GetSize() * 0.5f; } // 各軸の半分の長さを返す
	bool IsValid() const { return minPosition.x <= maxPosition.x && minPosition.y <= maxPosition.y && minPosition.z <= maxPosition.z; } // 最小座標が最大座標を追い越していないか確認する

	Vector3 minPosition{ Vector3::Zero };
	Vector3 maxPosition{ Vector3::Zero };
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
