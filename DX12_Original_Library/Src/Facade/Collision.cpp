#include "Collision.h"
#include "../Collision/CollisionSystem.h"

namespace
{
	CollisionSystem collisionSystem{};
}

bool Collision::Intersect(Rect _rect01, Rect _rect02)
{
	return collisionSystem.Intersect(_rect01, _rect02);
}

bool Collision::Intersect(AABB _cube01, AABB _cube02)
{
	return collisionSystem.Intersect(_cube01, _cube02);
}
