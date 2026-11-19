#include "../Physics/PhysicsSystem.h"
#include "PhysicsInternal.h"
#include "Physics.h"

namespace
{
	PhysicsSystem physicsSystem{}; // 物理管理システム
}

void PhysicsInternal::Initialize()
{
	physicsSystem.Setup();
}

void PhysicsInternal::Finish()
{
	physicsSystem.Shutdown();
}

BodyHandle Physics::CreateBody(const BodyDesc& _desc)
{
	return physicsSystem.CreateBody(_desc);
}

bool Physics::DestroyBody(BodyHandle _handle)
{
	return physicsSystem.DestroyBody(_handle);
}

void Physics::Step(float _deltaTime)
{
	physicsSystem.Step(_deltaTime);
}

Vector3 Physics::GetPosition(BodyHandle _handle)
{
	return physicsSystem.GetPosition(_handle);
}

Quaternion Physics::GetRotation(BodyHandle _handle)
{
	return physicsSystem.GetRotation(_handle);
}

Vector3 Physics::GetVelocity(BodyHandle _handle)
{
	return physicsSystem.GetVelocity(_handle);
}

BodyState Physics::GetState(BodyHandle _handle)
{
	return physicsSystem.GetState(_handle);
}

bool Physics::SetPosition(BodyHandle _handle, Vector3 _position)
{
	return physicsSystem.SetPosition(_handle, _position);
}

bool Physics::SetRotation(BodyHandle _handle, Quaternion _rotation)
{
	return physicsSystem.SetRotation(_handle, _rotation);
}

bool Physics::SetVelocity(BodyHandle _handle, Vector3 _velocity)
{
	return physicsSystem.SetVelocity(_handle, _velocity);
}
