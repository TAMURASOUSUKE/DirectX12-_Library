#pragma once
#include "../Math/Vector/Vector3.h"
#include "PhysicsInternalType.h"

// 計算の解決のヘルパー
namespace PhysicsSolver 
{
	// AとBへ逆向きに同じ力積を与える(速度の変化 = 力積 × 質量の逆数)
	void ApplyImpulse(BodyData& _bodyA, BodyData& _bodyB, const Vector3& _impulse);

	// 法線に垂直な向きを2つ作る(摩擦をかける向き)
	void MakeTangents(const Vector3& _normal, Vector3& _outTangent1, Vector3& _outTangent2);
}

