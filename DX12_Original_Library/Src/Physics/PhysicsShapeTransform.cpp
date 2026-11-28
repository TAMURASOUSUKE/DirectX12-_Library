#include "../Math/Quaternion/Quaternion.h"
#include "PhysicsShapeTransform.h"

Vector3 PhysicsShapeTransform::ToWorldPoint(const Vector3& _localPoint, const BodyData& _body)
{
	return _body.position + _body.rotation.RotateVector(_localPoint);
}

Sphere PhysicsShapeTransform::ToWorldShape(const Sphere& _sphere, const BodyData& _body)
{
	return Sphere{ ToWorldPoint(_sphere.center, _body), _sphere.radius };
}

Capsule PhysicsShapeTransform::ToWorldShape(const Capsule& _capsule, const BodyData& _body)
{
	return Capsule{ ToWorldPoint(_capsule.startPos, _body), ToWorldPoint(_capsule.endPos, _body), _capsule.radius };
}

Box PhysicsShapeTransform::ToWorldShape(const Box& _box, const BodyData& _body)
{
	// 箱自身の回転を先に掛け、その後にBodyの回転を掛ける
	return Box{ ToWorldPoint(_box.center, _body), _box.halfSize, _body.rotation * _box.rotation };
}

TriangleMeshShape PhysicsShapeTransform::ToWorldShape(const TriangleMeshShape& _mesh, const BodyData& _body)
{
	return _mesh;
}

HeightFieldShape PhysicsShapeTransform::ToWorldShape(const HeightFieldShape& _heightField, const BodyData& _body)
{
	return _heightField;
}

Shape PhysicsShapeTransform::ToWorldShape(const BodyData& _body)
{
	return std::visit([&_body](const auto& _shape) { return Shape{ ToWorldShape(_shape, _body) }; }, _body.shape);
}

Vector3 PhysicsShapeTransform::ToLocalPoint(const Vector3& _worldPoint, const BodyData& _body)
{
	const Quaternion inverseRotation{ Vector4{ -_body.rotation.x, -_body.rotation.y, -_body.rotation.z, _body.rotation.w } };
	return inverseRotation.RotateVector(_worldPoint - _body.position);
}

Shape PhysicsShapeTransform::ToMeshLocalShape(const Sphere& _sphere, const BodyData& _owner, const BodyData& _meshBody)
{
	const Sphere world{ ToWorldShape(_sphere, _owner) };
	return Shape{ Sphere{ ToLocalPoint(world.center, _meshBody), world.radius } };
}

Shape PhysicsShapeTransform::ToMeshLocalShape(const Capsule& _capsule, const BodyData& _owner, const BodyData& _meshBody)
{
	const Capsule world{ ToWorldShape(_capsule, _owner) };
	return Shape{ Capsule{ ToLocalPoint(world.startPos, _meshBody), ToLocalPoint(world.endPos, _meshBody), world.radius } };
}

Shape PhysicsShapeTransform::ToMeshLocalShape(const Box& _box, const BodyData& _owner, const BodyData& _meshBody)
{
	const Box world{ ToWorldShape(_box, _owner) };
	// 回転は相手の逆回転を後から掛ける
	const Quaternion inverseRotation{ Vector4{ -_meshBody.rotation.x, -_meshBody.rotation.y, -_meshBody.rotation.z, _meshBody.rotation.w } };
	return Shape{ Box{ ToLocalPoint(world.center, _meshBody), world.halfSize, inverseRotation * world.rotation } };
}

Shape PhysicsShapeTransform::ToMeshLocalShape(const TriangleMeshShape& _mesh, const BodyData& _owner, const BodyData& _meshBody)
{
	return Shape{ _mesh };
}

Shape PhysicsShapeTransform::ToMeshLocalShape(const HeightFieldShape& _heightField, const BodyData& _owner, const BodyData& _meshBody)
{
	return Shape{ _heightField };
}

PhysicsShapeTransform::ShapeRangeXZ PhysicsShapeTransform::GetRangeXZ(const Sphere& _sphere)
{
	return ShapeRangeXZ{ _sphere.center.x - _sphere.radius, _sphere.center.x + _sphere.radius, _sphere.center.z - _sphere.radius, _sphere.center.z + _sphere.radius };
}

PhysicsShapeTransform::ShapeRangeXZ PhysicsShapeTransform::GetRangeXZ(const Capsule& _capsule)
{
	// 両端の小さい方から半径を引き 大きい方へ半径を足す
	return ShapeRangeXZ{
		(std::min)(_capsule.startPos.x, _capsule.endPos.x) - _capsule.radius, (std::max)(_capsule.startPos.x, _capsule.endPos.x) + _capsule.radius,
		(std::min)(_capsule.startPos.z, _capsule.endPos.z) - _capsule.radius, (std::max)(_capsule.startPos.z, _capsule.endPos.z) + _capsule.radius };
}

PhysicsShapeTransform::ShapeRangeXZ PhysicsShapeTransform::GetRangeXZ(const Box& _box)
{
	// 箱の3本の軸をXとZへ投影した長さの合計が その向きの半径になる
	const Vector3 axisX{ _box.rotation.RotateVector(Vector3::Right) };
	const Vector3 axisY{ _box.rotation.RotateVector(Vector3::Up) };
	const Vector3 axisZ{ _box.rotation.RotateVector(Vector3::Forward) };
	const float radiusX{ _box.halfSize.x * std::abs(axisX.x) + _box.halfSize.y * std::abs(axisY.x) + _box.halfSize.z * std::abs(axisZ.x) };
	const float radiusZ{ _box.halfSize.x * std::abs(axisX.z) + _box.halfSize.y * std::abs(axisY.z) + _box.halfSize.z * std::abs(axisZ.z) };
	return ShapeRangeXZ{ _box.center.x - radiusX, _box.center.x + radiusX, _box.center.z - radiusZ, _box.center.z + radiusZ };
}

PhysicsShapeTransform::ShapeRangeXZ PhysicsShapeTransform::GetRangeXZ(const TriangleMeshShape& _mesh)
{
	return ShapeRangeXZ{};
}

PhysicsShapeTransform::ShapeRangeXZ PhysicsShapeTransform::GetRangeXZ(const HeightFieldShape& _heightField)
{
	return ShapeRangeXZ{};
}
