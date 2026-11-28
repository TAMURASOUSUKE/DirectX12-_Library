#include "PhysicsSolver.h"

void PhysicsSolver::ApplyImpulse(BodyData& _bodyA, BodyData& _bodyB, const Vector3& _impulse)
{
	_bodyA.velocity -= _impulse * _bodyA.massInverse;
	_bodyB.velocity += _impulse * _bodyB.massInverse;
}

void PhysicsSolver::MakeTangents(const Vector3& _normal, Vector3& _outTangent1, Vector3& _outTangent2)
{
	Vector3 tangent{ Vector3::Cross(_normal, Vector3::Up) };
	// 法線が上下を向いていると外積が0になるので別の軸を使う
	if (tangent.LengthSquared() <= Math::EPSILON) tangent = Vector3::Cross(_normal, Vector3::Right);
	tangent.Normalize();
	_outTangent1 = tangent;
	_outTangent2 = Vector3::Cross(_normal, tangent);
}
