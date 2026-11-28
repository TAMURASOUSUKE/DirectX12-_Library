#pragma once
#include "../Collision/CollisionSystem.h"
#include "../Math/Vector/Vector3.h"
#include "../Collision/Collider.h"
#include "PhysicsInternalType.h"
#include "PhysicsType.h"

// 座標の変換系のヘルパーを作成する
namespace PhysicsShapeTransform 
{
	// 形がXの向きとZの向きに占める範囲
	struct ShapeRangeXZ
	{
		float minX{ 0.0f }; // Xの最小
		float maxX{ 0.0f }; // Xの最大
		float minZ{ 0.0f }; // Zの最小
		float maxZ{ 0.0f }; // Zの最大
	};

	// 形の中の相対位置をBodyの位置と回転でワールドへ直す
	Vector3 ToWorldPoint(const Vector3& _localPoint, const BodyData& _body);
	Sphere ToWorldShape(const Sphere& _sphere, const BodyData& _body);
	Capsule ToWorldShape(const Capsule& _capsule, const BodyData& _body);
	Box ToWorldShape(const Box& _box, const BodyData& _body);

	// 三角形の集まりはハンドルを持つだけなのでそのまま返す(三角形は調べる側でローカルのまま使う)
	TriangleMeshShape ToWorldShape(const TriangleMeshShape& _mesh, const BodyData& _body);

	// 高さの格子はハンドルを持つだけなのでそのまま返す
	HeightFieldShape ToWorldShape(const HeightFieldShape& _heightField, const BodyData& _body);

	// Bodyが持っている形(variant)をワールドへ直して返す
	Shape ToWorldShape(const BodyData& _body);

	// ワールドの位置を相手のBodyのローカルへ移す(位置を引いてから逆回転)
	Vector3 ToLocalPoint(const Vector3& _worldPoint, const BodyData& _body);

	// 形をワールドへ直してから三角形の集まりのBodyのローカルへ移す
	Shape ToMeshLocalShape(const Sphere& _sphere, const BodyData& _owner, const BodyData& _meshBody);
	Shape ToMeshLocalShape(const Capsule& _capsule, const BodyData& _owner, const BodyData& _meshBody);
	Shape ToMeshLocalShape(const Box& _box, const BodyData& _owner, const BodyData& _meshBody);

	// 三角形の集まり同士は調べないので呼ばれない(std::visitが全ての型を求めるので置く)
	Shape ToMeshLocalShape(const TriangleMeshShape& _mesh, const BodyData& _owner, const BodyData& _meshBody);

	// 動かない形同士は調べないので呼ばれない(std::visitが全ての型を求めるので置く)
	Shape ToMeshLocalShape(const HeightFieldShape& _heightField, const BodyData& _owner, const BodyData& _meshBody);

	// 形ごとにXとZの範囲を求める(高さの格子のどの升と重なるかを調べるのに使う)
	ShapeRangeXZ GetRangeXZ(const Sphere& _sphere);
	ShapeRangeXZ GetRangeXZ(const Capsule& _capsule);
	ShapeRangeXZ GetRangeXZ(const Box& _box);

	// 動かない形同士は調べないので呼ばれない(std::visitが全ての型を求めるので置く)
	ShapeRangeXZ GetRangeXZ(const TriangleMeshShape& _mesh);
	ShapeRangeXZ GetRangeXZ(const HeightFieldShape& _heightField);

	// 表に入れる関数の型(9個とも同じ形にそろえる)
	using ContactFunction = bool(*)(CollisionSystem& _collisionSystem, const Shape& _shapeA, const Shape& _shapeB, ShapeToShapeResult& _result);

	// そのまま呼ぶ : variantからAの型とBの型を取り出して計算する
	template<typename ShapeTypeA, typename ShapeTypeB>
	bool CallContact(CollisionSystem& _collisionSystem, const Shape& _shapeA, const Shape& _shapeB, ShapeToShapeResult& _result)
	{
		return _collisionSystem.ComputeShapeContact(std::get<ShapeTypeA>(_shapeA), std::get<ShapeTypeB>(_shapeB), _result);
	}

	// 入れ替えて呼ぶ : 用意してある関数は(B, A)の並びなので逆にして呼び、法線の符号を反転する
	template<typename ShapeTypeA, typename ShapeTypeB>
	bool CallContactSwapped(CollisionSystem& _collisionSystem, const Shape& _shapeA, const Shape& _shapeB, ShapeToShapeResult& _result)
	{
		if (!_collisionSystem.ComputeShapeContact(std::get<ShapeTypeB>(_shapeB), std::get<ShapeTypeA>(_shapeA), _result)) return false;
		_result.normal = -_result.normal; // 法線はA->B 点は2つの表面の真ん中なので動かない
		return true;
	}
}
