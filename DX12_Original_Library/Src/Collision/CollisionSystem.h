#pragma once
#include "Collider.h"

// 衝突や接触判定を行う
class CollisionSystem
{
public:
	// 接触判定
	bool Intersect(Rect _rect01, Rect _rect02); // 矩形と矩形
	bool Intersect(AABB _cube01, AABB _cube02); // 箱と箱

private:

};
