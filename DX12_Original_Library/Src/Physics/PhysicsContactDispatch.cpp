#include "PhysicsContactDispatch.h"

bool PhysicsContactDispatch::ComputeTriangleContact(CollisionSystem& _collisionSystem, const Sphere& _sphere, const Triangle& _triangle, ShapeToShapeResult& _result)
{
	return _collisionSystem.ComputeShapeContact(_sphere, _triangle, _result);
}

bool PhysicsContactDispatch::ComputeTriangleContact(CollisionSystem& _collisionSystem, const Capsule& _capsule, const Triangle& _triangle, ShapeToShapeResult& _result)
{
	return _collisionSystem.ComputeShapeContact(_capsule, _triangle, _result);
}

bool PhysicsContactDispatch::ComputeTriangleContact(CollisionSystem& _collisionSystem, const Box& _box, const Triangle& _triangle, ShapeToShapeResult& _result)
{
	return _collisionSystem.ComputeShapeContact(_box, _triangle, _result);
}

bool PhysicsContactDispatch::ComputeTriangleContact(CollisionSystem& _collisionSystem, const TriangleMeshShape& _mesh, const Triangle& _triangle, ShapeToShapeResult& _result)
{
	return false;
}

bool PhysicsContactDispatch::ComputeTriangleContact(CollisionSystem& _collisionSystem, const HeightFieldShape& _heightField, const Triangle& _triangle, ShapeToShapeResult& _result)
{
	return false;
}
