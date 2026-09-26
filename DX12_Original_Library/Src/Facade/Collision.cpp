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

bool Collision::Intersect(Capsule _capsule01, Capsule _capsule02)
{
	return collisionSystem.Intersect(_capsule01, _capsule02);
}

bool Collision::Intersect(Capsule _capsule, Box _box)
{
	return collisionSystem.Intersect(_capsule, _box);
}

bool Collision::Intersect(Capsule _capsule, Sphere _sphere)
{
	return collisionSystem.Intersect(_capsule, _sphere);
}

bool Collision::Intersect(Box _box, Sphere _sphere) { return Intersect(_sphere, _box); }
bool Collision::Intersect(Box _box, Capsule _capsule) { return Intersect(_capsule, _box); }
bool Collision::Intersect(Sphere _sphere, Capsule _capsule) { return Intersect(_capsule, _sphere); }

bool Collision::ComputePushOut(Rect _movable, Rect _obstacle, Vector2& _outMove)
{
	return collisionSystem.ComputePushOut(_movable, _obstacle, _outMove);
}

bool Collision::ComputePushOut(Box _movable, Box _obstacle, Vector3& _outMove)
{
	return collisionSystem.ComputePushOut(_movable, _obstacle, _outMove);
}

bool Collision::ComputePushOut(Sphere _movable, Sphere _obstacle, Vector3& _outMove)
{
	return collisionSystem.ComputePushOut(_movable, _obstacle, _outMove);
}

bool Collision::ComputePushOut(Sphere _movable, Box _obstacle, Vector3& _outMove)
{
	return collisionSystem.ComputePushOut(_movable, _obstacle, _outMove);
}

bool Collision::ComputePushOut(Box _movable, Sphere _obstacle, Vector3& _outMove)
{
	Vector3 moveSphere{};
	const bool hit{ collisionSystem.ComputePushOut(_obstacle, _movable, moveSphere) };
	_outMove = -moveSphere;
	return hit;
}

bool Collision::ComputePushOut(Capsule _movable, Capsule _obstacle, Vector3& _outMove)
{
	return collisionSystem.ComputePushOut(_movable, _obstacle, _outMove);
}

bool Collision::ComputePushOut(Capsule _movable, Box _obstacle, Vector3& _outMove)
{
	return collisionSystem.ComputePushOut(_movable, _obstacle, _outMove);
}

bool Collision::ComputePushOut(Box _movable, Capsule _obstacle, Vector3& _outMove)
{
	Vector3 moveCapsule{};
	const bool hit{ collisionSystem.ComputePushOut(_obstacle, _movable, moveCapsule) };
	_outMove = -moveCapsule;
	return hit;
}

bool Collision::ComputePushOut(Capsule _movable, Sphere _obstacle, Vector3& _outMove)
{
	return collisionSystem.ComputePushOut(_movable, _obstacle, _outMove);
}

bool Collision::ComputePushOut(Sphere _movable, Capsule _obstacle, Vector3& _outMove)
{
	Vector3 moveCapsule{};
	const bool hit{ collisionSystem.ComputePushOut(_obstacle, _movable, moveCapsule) };
	_outMove = -moveCapsule;
	return hit;
}

