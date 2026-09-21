#pragma once
#include "Collider.h"

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

private:

};
