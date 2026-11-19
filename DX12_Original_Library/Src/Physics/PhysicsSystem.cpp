#include "PhysicsSystem.h"

void PhysicsSystem::Setup()
{

}

void PhysicsSystem::Shutdown()
{

}

BodyHandle PhysicsSystem::CreateBody(BodyDesc _desc)
{

}

bool PhysicsSystem::DestroyBody(BodyHandle _handle)
{

}

void PhysicsSystem::Step(float _deltaTime)
{

}

Vector3 PhysicsSystem::GetPosition(BodyHandle _handle)
{

}

Quaternion PhysicsSystem::GetRotation(BodyHandle _handle)
{

}

Vector3 PhysicsSystem::GetVelocity(BodyHandle _handle)
{

}

BodyState PhysicsSystem::GetState(BodyHandle _handle)
{

}

bool PhysicsSystem::SetPosition(BodyHandle _handle, Vector3 _position)
{

}

bool PhysicsSystem::SetRotation(BodyHandle _handle, Quaternion _rotation)
{

}

bool PhysicsSystem::SetVelocity(BodyHandle _handle, Vector3 _velocity)
{

}

BodyData* PhysicsSystem::Lookup(BodyHandle _handle)
{

}
