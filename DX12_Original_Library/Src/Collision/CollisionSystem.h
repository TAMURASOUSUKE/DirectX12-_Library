#pragma once
#include "Collider.h"
#include "CollisionType.h"

// 衝突や接触判定を行う
class CollisionSystem
{
public:
	// 接触判定
	bool Intersect(Rect _rect01, Rect _rect02); // 矩形と矩形
	bool Intersect(Box _cube01, Box _02); // 箱と箱
	bool Intersect(Sphere _sphere01, Sphere _sphere02); // 球と球
	bool Intersect(Sphere _sphere, Box _box); // 球と箱
	bool Intersect(Capsule _capsule01, Capsule _capsule02); // カプセルとカプセル
	bool Intersect(Capsule _capsule, Box _box); // カプセルと箱
	bool Intersect(Capsule _capsule, Sphere _sphere); // カプセルと球

	// 第1引数を固定された第2引数の外へ出す移動量を返す 接するだけならtrueとゼロ、非接触・不正値ならfalseとゼロを返す。
	bool ComputePushOut(Rect _movable, Rect _obstacle, Vector2& _outMove);
	bool ComputePushOut(Box _movable, Box _obstacle, Vector3& _outMove);
	bool ComputePushOut(Sphere _movable, Sphere _obstacle, Vector3& _outMove);
	bool ComputePushOut(Sphere _movable, Box _obstacle, Vector3& _outMove);
	bool ComputePushOut(Capsule _movable, Capsule _obstacle, Vector3& _outMove);
	bool ComputePushOut(Capsule _movable, Box _obstacle, Vector3& _outMove);
	bool ComputePushOut(Capsule _movable, Sphere _obstacle, Vector3& _outMove);

	// 型で返す
	bool ComputeShapeContact(Sphere _sphereA, Sphere _sphereB, ShapeToShapeResult& _result);
	bool ComputeShapeContact(Capsule _capsuleA, Sphere _sphereB, ShapeToShapeResult& _result);
	bool ComputeShapeContact(Capsule _capsuleA, Capsule _capsuleB, ShapeToShapeResult& _result);
	bool ComputeShapeContact(Sphere _sphereA, Box _boxB, ShapeToShapeResult& _result);
	bool ComputeShapeContact(Capsule _capsuleA, Box _boxB, ShapeToShapeResult& _result);
	bool ComputeShapeContact(Box _boxA, Box _boxB, ShapeToShapeResult& _result);
	bool ComputeShapeContact(Sphere _sphereA, Triangle _triangleB, ShapeToShapeResult& _result);
	bool ComputeShapeContact(Capsule _capsuleA, Triangle _triangleB, ShapeToShapeResult& _result);
	bool ComputeShapeContact(Box _boxA, Triangle _triangleB, ShapeToShapeResult& _result);


private:

};
