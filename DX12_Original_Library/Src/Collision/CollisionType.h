#pragma once
#include <array>
#include "CollisionConstant.h"
#include "../Math/Vector/Vector3.h"

// ユーザーに提供する接触型の作成

// 接触点
struct CollisionPoint
{
	Vector3 worldPosition{ Vector3::Zero }; // 位置
	float depth{ 0.0f }; // 深さ
};

// 形同士の結果
struct ShapeToShapeResult
{
	Vector3 normal{ Vector3::Zero }; // 法線 A->Bの方向
	std::array<CollisionPoint, MAX_CONTACT_POINT_COUNT> points{}; // 点の配列
	int pointCount{ 0 }; // 入っている点の数
};

// Ryaと形状のあたり判定
struct RayToShapeResult
{
	Vector3 worldPosition{ Vector3::Zero }; // 当たった時の位置
	Vector3 normal{ Vector3::Zero }; // 当たった面の向き
	float distance{ 0.0f }; // 視点からの距離
};
