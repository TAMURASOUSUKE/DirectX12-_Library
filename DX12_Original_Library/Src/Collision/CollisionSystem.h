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
	bool Intersect(Sphere _sphere, Box _box); // 球と

private:

};
