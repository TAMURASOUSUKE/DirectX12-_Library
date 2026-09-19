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

bool Collision::Intersect(Box _cube01, Box _cube02)
{
	return collisionSystem.Intersect(_cube01, _cube02);
}

bool Collision::Intersect(Sphere _sphere01, Sphere _sphere02)
{
	return collisionSystem.Intersect(_sphere01, _sphere02);
}

bool Collision::Intersect(Sphere _sphere, Box _box)
{
	return collisionSystem.Intersect(_sphere, _box);
}
