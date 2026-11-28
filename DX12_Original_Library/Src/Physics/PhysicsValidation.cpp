#include <cmath>
#include "../Math/MathConstant.h"
#include "PhysicsValidation.h"

bool PhysicsValidation::IsFinite(const Vector3& _value)
{
	return std::isfinite(_value.x) && std::isfinite(_value.y) && std::isfinite(_value.z);
}

bool PhysicsValidation::IsValidRotation(const Quaternion& _rotation)
{
	const bool isValidVector{ std::isfinite(_rotation.x) && std::isfinite(_rotation.y) && std::isfinite(_rotation.z) && std::isfinite(_rotation.w) }; // 4成分部分の有限値確認
	const bool isValidLength{ std::abs(_rotation.LengthSquared() - 1.0f) <= Math::EPSILON }; // 長さが1か確認
	return isValidVector && isValidLength;
}

bool PhysicsValidation::IsValidDesc(const BodyDesc& _desc)
{
	// 入力された形状のIsValid関数を呼び出すvisit
	const bool isValidShape{ std::visit([](const auto& _shape) { return _shape.IsValid(); }, _desc.shapeData) };
	if (!isValidShape) return false;

	if (!IsFinite(_desc.position) || !IsValidRotation(_desc.rotation)) return false;
	if (!IsFinite(_desc.velocity) || !IsFinite(_desc.angularVelocity)) return false;
	if (!std::isfinite(_desc.friction) || _desc.friction < 0.0f) return  false; // 摩擦は0未満になってはならない

	// 逆数を取るので質量は正の値だけ
	const bool isDynamic{ _desc.motionType == MotionType::Dynamic };
	if (isDynamic && !(std::isfinite(_desc.mass) && _desc.mass > 0.0f)) return false;
	return true;
}

BodyData PhysicsValidation::MakeBodyData(const BodyDesc& _desc)
{
	const bool isStatic{ _desc.motionType == MotionType::Static };
	const bool isDynamic{ _desc.motionType == MotionType::Dynamic };
	BodyData data{};
	data.shape = _desc.shapeData;
	data.position = _desc.position;
	data.rotation = _desc.rotation;
	data.velocity = isStatic ? Vector3::Zero : _desc.velocity; // Staticは速度を持たない
	data.massInverse = isDynamic ? 1.0f / _desc.mass : 0.0f; // 0は力では動かないことを表す
	data.angularVelocity = _desc.angularVelocity;
	data.friction = _desc.friction;
	data.motionType = _desc.motionType;
	data.isTrigger = _desc.isTrigger;
	data.isGravityEnabled = _desc.isGravityEnabled;
	data.layer = _desc.layer;
	data.collisionMask = _desc.collisionMask;
	data.userData = _desc.userData;
	return data;
}
