#pragma once
#include "../Collision/Collider.h"

 // 判定を行う機能を提供する名前空間
namespace Collision
{
	bool Intersect(Rect _rect01, Rect _rect02); // 矩形と矩形
	bool Intersect(Box _cube01, Box _cube02); // 箱と箱
	bool Intersect(Sphere _sphere01, Sphere _sphere02); // 球と球
	bool Intersect(Sphere _sphere, Box _box); // 球と箱
}
