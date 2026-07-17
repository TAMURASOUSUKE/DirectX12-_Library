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