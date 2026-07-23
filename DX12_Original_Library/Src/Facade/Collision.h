#pragma once
#include "../Collision/Collider.h"

 // 判定を行う機能を提供する名前空間
namespace Collision
{
	bool Intersect(Rect _rect01, Rect _rect02);
}