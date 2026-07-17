#include "../Math/TSMath.h"
#include "CollisionSystem.h"

bool CollisionSystem::Intersect(Rect _rect01, Rect _rect02)
{
	Vector2 min01{}; // 一つめの最小
	Vector2 max01{}; // 一つめの最大
	Vector2 min02{}; // 二つめの最小
	Vector2 max02{}; // 二つ目の最大
	// 判定を行う どれか一つでも重なっていなければ当たっていない
	if (max01.x < min02.x || min01.x > max02.x) return false;
	if (max01.y < min02.y || min01.y > max02.y) return false;
	return true;
}

