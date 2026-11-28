#pragma once
#include "../Collision/CollisionSystem.h"
#include "../Collision/Collider.h"
#include "PhysicsType.h"

// 接触の呼び分けを担当するヘルパー群
namespace PhysicsContactDispatch
{
	// 形と三角形1枚の接触を計算する(形の型ごとに用意した関数を呼び分ける)
	bool ComputeTriangleContact(CollisionSystem& _collisionSystem, const Sphere& _sphere, const Triangle& _triangle, ShapeToShapeResult& _result);
	bool ComputeTriangleContact(CollisionSystem& _collisionSystem, const Capsule& _capsule, const Triangle& _triangle, ShapeToShapeResult& _result);
	bool ComputeTriangleContact(CollisionSystem& _collisionSystem, const Box& _box, const Triangle& _triangle, ShapeToShapeResult& _result);

	// 三角形の集まり同士は調べないので呼ばれない(std::visitが全ての型を求めるので置く)
	bool ComputeTriangleContact(CollisionSystem& _collisionSystem, const TriangleMeshShape& _mesh, const Triangle& _triangle, ShapeToShapeResult& _result);
	bool ComputeTriangleContact(CollisionSystem& _collisionSystem, const HeightFieldShape& _heightField, const Triangle& _triangle, ShapeToShapeResult& _result);
}
