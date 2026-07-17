#pragma once
#include "Collider.h"

// 衝突や接触判定を行う
class CollisionSystem
{
public:
	// 接触判定
	bool Intersect(Rect _rect01, Rect _rect02); // 今後拡張していきこの関数は消える

private:

};