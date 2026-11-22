#include <cmath>
#include <variant>
#include <array>
#include <algorithm>
#include "../Core/Handle/HandlePacking.h"
#include "../Debug/DebugLogs.h"
#include "PhysicsConstant.h"
#include "PhysicsSystem.h"

namespace
{
	// 有限値かをVector3で直接調べるヘルパー
	bool IsFinite(const Vector3& _value)
	{
		return std::isfinite(_value.x) && std::isfinite(_value.y) && std::isfinite(_value.z);
	}

	// 有限で長さ1の回転か
	bool IsValidRotation(const Quaternion& _rotation)
	{
		const bool isValidVector{ std::isfinite(_rotation.x) && std::isfinite(_rotation.y) && std::isfinite(_rotation.z) && std::isfinite(_rotation.w) }; // 4成分部分の有限値確認
		const bool isValidLength{ std::abs(_rotation.LengthSquared() - 1.0f) <= Math::EPSILON }; // 長さが1か確認
		return isValidVector && isValidLength; 
	}

	// 作成に使える設定か
	bool IsValidDesc(const BodyDesc& _desc)
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

	// 設定から内部データを作る
	BodyData MakeBodyData(const BodyDesc& _desc)
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

	// 形の中の相対位置をBodyの位置と回転でワールドへ直す
	Vector3 ToWorldPoint(const Vector3& _localPoint, const BodyData& _body)
	{
		return _body.position + _body.rotation.RotateVector(_localPoint);
	}

	Sphere ToWorldShape(const Sphere& _sphere, const BodyData& _body)
	{
		return Sphere{ ToWorldPoint(_sphere.center, _body), _sphere.radius };
	}

	Capsule ToWorldShape(const Capsule& _capsule, const BodyData& _body)
	{
		return Capsule{ ToWorldPoint(_capsule.startPos, _body), ToWorldPoint(_capsule.endPos, _body), _capsule.radius };
	}

	Box ToWorldShape(const Box& _box, const BodyData& _body)
	{
		// 箱自身の回転を先に掛け、その後にBodyの回転を掛ける
		return Box{ ToWorldPoint(_box.center, _body), _box.halfSize, _body.rotation * _box.rotation };
	}

	// 三角形の集まりはハンドルを持つだけなのでそのまま返す(三角形は調べる側でローカルのまま使う)
	TriangleMeshShape ToWorldShape(const TriangleMeshShape& _mesh, const BodyData& _body)
	{
		return _mesh;
	}

	// 高さの格子はハンドルを持つだけなのでそのまま返す
	HeightFieldShape ToWorldShape(const HeightFieldShape& _heightField, const BodyData& _body)
	{
		return _heightField;
	}

	// ワールドの位置を相手のBodyのローカルへ移す(位置を引いてから逆回転)
	Vector3 ToLocalPoint(const Vector3& _worldPoint, const BodyData& _body)
	{
		const Quaternion inverseRotation{ Vector4{ -_body.rotation.x, -_body.rotation.y, -_body.rotation.z, _body.rotation.w } };
		return inverseRotation.RotateVector(_worldPoint - _body.position);
	}

	// 一つのレイヤーが一つのCollisionMaskに入っているかを返す
	bool IsLayerMatched(std::uint32_t _layer, std::uint32_t _collisionMask)
	{
		// 渡されたMaskの中に自分が入っているか
		return (_layer & _collisionMask) != 0;
	}

	// 形をワールドへ直してから三角形の集まりのBodyのローカルへ移す
	Shape ToMeshLocalShape(const Sphere& _sphere, const BodyData& _owner, const BodyData& _meshBody)
	{
		const Sphere world{ ToWorldShape(_sphere, _owner) };
		return Shape{ Sphere{ ToLocalPoint(world.center, _meshBody), world.radius } };
	}

	Shape ToMeshLocalShape(const Capsule& _capsule, const BodyData& _owner, const BodyData& _meshBody)
	{
		const Capsule world{ ToWorldShape(_capsule, _owner) };
		return Shape{ Capsule{ ToLocalPoint(world.startPos, _meshBody), ToLocalPoint(world.endPos, _meshBody), world.radius } };
	}

	Shape ToMeshLocalShape(const Box& _box, const BodyData& _owner, const BodyData& _meshBody)
	{
		const Box world{ ToWorldShape(_box, _owner) };
		// 回転は相手の逆回転を後から掛ける
		const Quaternion inverseRotation{ Vector4{ -_meshBody.rotation.x, -_meshBody.rotation.y, -_meshBody.rotation.z, _meshBody.rotation.w } };
		return Shape{ Box{ ToLocalPoint(world.center, _meshBody), world.halfSize, inverseRotation * world.rotation } };
	}

	// 三角形の集まり同士は調べないので呼ばれない(std::visitが全ての型を求めるので置く)
	Shape ToMeshLocalShape(const TriangleMeshShape& _mesh, const BodyData& _owner, const BodyData& _meshBody)
	{
		return Shape{ _mesh };
	}

	// 形と三角形1枚の接触を計算する(形の型ごとに用意した関数を呼び分ける)
	bool ComputeTriangleContact(CollisionSystem& _collisionSystem, const Sphere& _sphere, const Triangle& _triangle, ShapeToShapeResult& _result)
	{
		return _collisionSystem.ComputeShapeContact(_sphere, _triangle, _result);
	}

	bool ComputeTriangleContact(CollisionSystem& _collisionSystem, const Capsule& _capsule, const Triangle& _triangle, ShapeToShapeResult& _result)
	{
		return _collisionSystem.ComputeShapeContact(_capsule, _triangle, _result);
	}

	bool ComputeTriangleContact(CollisionSystem& _collisionSystem, const Box& _box, const Triangle& _triangle, ShapeToShapeResult& _result)
	{
		return _collisionSystem.ComputeShapeContact(_box, _triangle, _result);
	}

	// 三角形の集まり同士は調べないので呼ばれない(std::visitが全ての型を求めるので置く)
	bool ComputeTriangleContact(CollisionSystem& _collisionSystem, const TriangleMeshShape& _mesh, const Triangle& _triangle, ShapeToShapeResult& _result)
	{
		return false;
	}

	// 動かない形同士は調べないので呼ばれない(std::visitが全ての型を求めるので置く)
	Shape ToMeshLocalShape(const HeightFieldShape& _heightField, const BodyData& _owner, const BodyData& _meshBody)
	{
		return Shape{ _heightField };
	}

	bool ComputeTriangleContact(CollisionSystem& _collisionSystem, const HeightFieldShape& _heightField, const Triangle& _triangle, ShapeToShapeResult& _result)
	{
		return false;
	}

	// 形がXの向きとZの向きに占める範囲
	struct ShapeRangeXZ
	{
		float minX{ 0.0f }; // Xの最小
		float maxX{ 0.0f }; // Xの最大
		float minZ{ 0.0f }; // Zの最小
		float maxZ{ 0.0f }; // Zの最大
	};

	// 形ごとにXとZの範囲を求める(高さの格子のどの升と重なるかを調べるのに使う)
	ShapeRangeXZ GetRangeXZ(const Sphere& _sphere)
	{
		return ShapeRangeXZ{ _sphere.center.x - _sphere.radius, _sphere.center.x + _sphere.radius, _sphere.center.z - _sphere.radius, _sphere.center.z + _sphere.radius };
	}

	ShapeRangeXZ GetRangeXZ(const Capsule& _capsule)
	{
		// 両端の小さい方から半径を引き 大きい方へ半径を足す
		return ShapeRangeXZ{
			(std::min)(_capsule.startPos.x, _capsule.endPos.x) - _capsule.radius, (std::max)(_capsule.startPos.x, _capsule.endPos.x) + _capsule.radius,
			(std::min)(_capsule.startPos.z, _capsule.endPos.z) - _capsule.radius, (std::max)(_capsule.startPos.z, _capsule.endPos.z) + _capsule.radius };
	}

	ShapeRangeXZ GetRangeXZ(const Box& _box)
	{
		// 箱の3本の軸をXとZへ投影した長さの合計が その向きの半径になる
		const Vector3 axisX{ _box.rotation.RotateVector(Vector3::Right) };
		const Vector3 axisY{ _box.rotation.RotateVector(Vector3::Up) };
		const Vector3 axisZ{ _box.rotation.RotateVector(Vector3::Forward) };
		const float radiusX{ _box.halfSize.x * std::abs(axisX.x) + _box.halfSize.y * std::abs(axisY.x) + _box.halfSize.z * std::abs(axisZ.x) };
		const float radiusZ{ _box.halfSize.x * std::abs(axisX.z) + _box.halfSize.y * std::abs(axisY.z) + _box.halfSize.z * std::abs(axisZ.z) };
		return ShapeRangeXZ{ _box.center.x - radiusX, _box.center.x + radiusX, _box.center.z - radiusZ, _box.center.z + radiusZ };
	}

	// 動かない形同士は調べないので呼ばれない(std::visitが全ての型を求めるので置く)
	ShapeRangeXZ GetRangeXZ(const TriangleMeshShape& _mesh)
	{
		return ShapeRangeXZ{};
	}

	ShapeRangeXZ GetRangeXZ(const HeightFieldShape& _heightField)
	{
		return ShapeRangeXZ{};
	}

	// Bodyが持っている形(variant)をワールドへ直して返す
	Shape ToWorldShape(const BodyData& _body)
	{
		return std::visit([&_body](const auto& _shape) { return Shape{ ToWorldShape(_shape, _body) }; }, _body.shape);
	}

	// 表に入れる関数の型(9個とも同じ形にそろえる)
	using ContactFunction = bool(*)(CollisionSystem& _collisionSystem, const Shape& _shapeA, const Shape& _shapeB, ShapeToShapeResult& _result);

	// そのまま呼ぶ : variantからAの型とBの型を取り出して計算する
	template<typename ShapeTypeA, typename ShapeTypeB>
	bool CallContact(CollisionSystem& _collisionSystem, const Shape& _shapeA, const Shape& _shapeB, ShapeToShapeResult& _result)
	{
		return _collisionSystem.ComputeShapeContact(std::get<ShapeTypeA>(_shapeA), std::get<ShapeTypeB>(_shapeB), _result);
	}

	// 入れ替えて呼ぶ : 用意してある関数は(B, A)の並びなので逆にして呼び、法線の符号を反転する
	template<typename ShapeTypeA, typename ShapeTypeB>
	bool CallContactSwapped(CollisionSystem& _collisionSystem, const Shape& _shapeA, const Shape& _shapeB, ShapeToShapeResult& _result)
	{
		if (!_collisionSystem.ComputeShapeContact(std::get<ShapeTypeB>(_shapeB), std::get<ShapeTypeA>(_shapeA), _result)) return false;
		_result.normal = -_result.normal; // 法線はA->B 点は2つの表面の真ん中なので動かない
		return true;
	}

	// AとBへ逆向きに同じ力積を与える(速度の変化 = 力積 × 質量の逆数)
	void ApplyImpulse(BodyData& _bodyA, BodyData& _bodyB, const Vector3& _impulse)
	{
		_bodyA.velocity -= _impulse * _bodyA.massInverse;
		_bodyB.velocity += _impulse * _bodyB.massInverse;
	}

	// 法線に垂直な向きを2つ作る(摩擦をかける向き)
	void MakeTangents(const Vector3& _normal, Vector3& _outTangent1, Vector3& _outTangent2)
	{
		Vector3 tangent{ Vector3::Cross(_normal, Vector3::Up) };
		// 法線が上下を向いていると外積が0になるので別の軸を使う
		if (tangent.LengthSquared() <= Math::EPSILON) tangent = Vector3::Cross(_normal, Vector3::Right);
		tangent.Normalize();
		_outTangent1 = tangent;
		_outTangent2 = Vector3::Cross(_normal, tangent);
	}
}

namespace
{
	// 形の番号2つで関数を選ぶ表 [Aの番号][Bの番号]  番号はShapeの並び 箱=0 カプセル=1 球=2
	const std::array<std::array<ContactFunction, 3>, 3> contactFunctionTable{ {
		{ &CallContact<Box, Box>,     &CallContactSwapped<Box, Capsule>,    &CallContactSwapped<Box, Sphere> },
		{ &CallContact<Capsule, Box>, &CallContact<Capsule, Capsule>,       &CallContact<Capsule, Sphere> },
		{ &CallContact<Sphere, Box>,  &CallContactSwapped<Sphere, Capsule>, &CallContact<Sphere, Sphere> },
	} };

}

void PhysicsSystem::Setup()
{
	Shutdown(); // 再初期化対策
	slots.reserve(MAX_BODY_COUNT); // 最大容量をあらかじめ確保(作成時に再確保させない)
	triangleMeshSlots.reserve(MAX_TRIANGLE_MESH_COUNT);
	heightFieldSlots.reserve(MAX_HEIGHT_FIELD_COUNT);
	contacts.reserve(MAX_CONTACT_COUNT); 
	contactSolveData.reserve(MAX_CONTACT_COUNT);
}

void PhysicsSystem::Shutdown()
{
	slots.clear();
	contacts.clear();
	triangleMeshSlots.clear();
	while (!triangleMeshFreeList.empty())
	{
		triangleMeshFreeList.pop();
	}
	heightFieldSlots.clear();
	while (!heightFieldFreeList.empty())
	{
		heightFieldFreeList.pop();
	}
	contactSolveData.clear();
	while (!freeList.empty())
	{
		freeList.pop();
	}
}

BodyHandle PhysicsSystem::CreateBody(const BodyDesc& _desc)
{
	if (!IsValidDesc(_desc))
	{
		DEBUG_LOG_ERROR("不正な設定構造体が渡されました\n");
		return BodyHandle{};
	}

	// 三角形の集まりは登録済みでStaticのときだけ使える
	if (_desc.shapeData.index() == TRIANGLE_MESH_SHAPE_INDEX)
	{
		const TriangleMeshShape& meshShape{ std::get<TriangleMeshShape>(_desc.shapeData) };
		if (!Lookup(meshShape.mesh) || _desc.motionType != MotionType::Static)
		{
			DEBUG_LOG_ERROR("三角形の集まりの設定が無効です\n");
			return BodyHandle{};
		}
	}

	// 高さの格子も登録済みでStaticのときだけ使える
	if (_desc.shapeData.index() == HEIGHT_FIELD_SHAPE_INDEX)
	{
		const HeightFieldShape& fieldShape{ std::get<HeightFieldShape>(_desc.shapeData) };
		if (!Lookup(fieldShape.heightField) || _desc.motionType != MotionType::Static)
		{
			DEBUG_LOG_ERROR("高さの格子の設定が無効です\n");
			return BodyHandle{};
		}
	}

	// 新しく確保できるか再利用できるか
	const bool canRegister{ !freeList.empty() || static_cast<int>(slots.size()) < MAX_BODY_COUNT };
	DEBUG_ASSERT(canRegister && "Bodyの登録上限に達しました\n");
	if (!canRegister) return BodyHandle{};

	int index{ 0 };
	if (!freeList.empty())
	{
		index = freeList.top(); // Destroyされたところから持ってくる
		freeList.pop();
		slots[index].data = MakeBodyData(_desc);
		slots[index].isAlive = true; // 世代はDestroyで上げてあるのでそのまま
	}
	else
	{
		index = static_cast<int>(slots.size()); // 空きがないなら新しく作る
		slots.push_back({ MakeBodyData(_desc), 0, true }); // 新しく確保したので世代は0
	}
	const int pack{ Pack(index, static_cast<int>(slots[index].generation)) };
	return BodyHandle{ PassKey{}, pack };
}

bool PhysicsSystem::DestroyBody(BodyHandle _handle)
{
	BodyData* data{ Lookup(_handle) };
	if (!data) return false;

	const int index{ UnpackIndex(_handle.GetRaw(PassKey{})) };
	slots[index].data = {};
	slots[index].generation++;
	slots[index].isAlive = false;
	freeList.push(index); // この位置を使えるようにする
	return true;
}

TriangleMeshHandle PhysicsSystem::CreateTriangleMesh(std::span<const Vector3> _vertices, std::span<const std::uint32_t> _indices)
{
	if (_indices.empty() || _indices.size() % 3 != 0)
	{
		DEBUG_LOG_ERROR("三角形の番号の数が無効です\n");
		return TriangleMeshHandle{};
	}

	// 新しく確保できるか再利用できるか
	const bool canRegister{ !triangleMeshFreeList.empty() || static_cast<int>(triangleMeshSlots.size()) < MAX_TRIANGLE_MESH_COUNT };
	if (!canRegister)
	{
		DEBUG_LOG_ERROR("三角形の集まりの登録上限に達しました\n");
		return TriangleMeshHandle{};
	}

	TriangleMeshData data{};
	data.triangles.reserve(_indices.size() / 3);
	for (std::size_t i = 0; i + 2 < _indices.size(); i += 3)
	{
		const std::uint32_t indexA{ _indices[i] };
		const std::uint32_t indexB{ _indices[i + 1] };
		const std::uint32_t indexC{ _indices[i + 2] };
		if (indexA >= _vertices.size() || indexB >= _vertices.size() || indexC >= _vertices.size())
		{
			DEBUG_LOG_ERROR("三角形の番号が頂点の数を超えています\n");
			return TriangleMeshHandle{};
		}

		const Triangle triangle{ _vertices[indexA], _vertices[indexB], _vertices[indexC] };
		// 面積が0の三角形や有限でない頂点が一つでもあれば登録しない
		if (!triangle.IsValid())
		{
			DEBUG_LOG_ERROR("面積が0または不正な三角形が含まれています\n");
			return TriangleMeshHandle{};
		}
		data.triangles.push_back(triangle);
	}

	int index{ 0 };
	if (!triangleMeshFreeList.empty())
	{
		index = triangleMeshFreeList.top(); // Destroyされたところから持ってくる
		triangleMeshFreeList.pop();
		triangleMeshSlots[index].data = data;
		triangleMeshSlots[index].isAlive = true; // 世代はDestroyで上げてあるのでそのまま
	}
	else
	{
		index = static_cast<int>(triangleMeshSlots.size()); // 空きがないなら新しく作る
		triangleMeshSlots.push_back({ data, 0, true }); // 新しく確保したので世代は0
	}
	const int pack{ Pack(index, static_cast<int>(triangleMeshSlots[index].generation)) };
	return TriangleMeshHandle{ PassKey{}, pack };
}

bool PhysicsSystem::DestroyTriangleMesh(TriangleMeshHandle _handle)
{
	TriangleMeshData* data{ Lookup(_handle) };
	if (!data) return false;

	// Bodyが使っている間は断る
	for (const BodySlot& slot : slots)
	{
		if (!slot.isAlive) continue;
		if (slot.data.shape.index() != TRIANGLE_MESH_SHAPE_INDEX) continue;
		if (std::get<TriangleMeshShape>(slot.data.shape).mesh == _handle)
		{
			DEBUG_LOG_ERROR("Bodyが使っている三角形の集まりは削除できません\n");
			return false;
		}
	}

	const int index{ UnpackIndex(_handle.GetRaw(PassKey{})) };
	triangleMeshSlots[index].data = {};
	triangleMeshSlots[index].generation++;
	triangleMeshSlots[index].isAlive = false;
	triangleMeshFreeList.push(index); // この位置を使えるようにする
	return true;
}

HeightFieldHandle PhysicsSystem::CreateHeightField(int _countX, int _countZ, float _cellSize, std::span<const float> _heights)
{
	// 升を1つ作るにはXとZそれぞれ2点以上が要る 升の大きさは正の有限値だけ
	const bool isValidSize{ _countX >= 2 && _countZ >= 2 && std::isfinite(_cellSize) && _cellSize > 0.0f };
	if (!isValidSize || _heights.size() != static_cast<std::size_t>(_countX) * static_cast<std::size_t>(_countZ))
	{
		DEBUG_LOG_ERROR("高さの格子の大きさが無効です\n");
		return HeightFieldHandle{};
	}
	for (const float height : _heights)
	{
		if (std::isfinite(height)) continue;
		DEBUG_LOG_ERROR("高さの格子に有限でない高さが含まれています\n");
		return HeightFieldHandle{};
	}

	// 新しく確保できるか再利用できるか
	const bool canRegister{ !heightFieldFreeList.empty() || static_cast<int>(heightFieldSlots.size()) < MAX_HEIGHT_FIELD_COUNT };
	if (!canRegister)
	{
		DEBUG_LOG_ERROR("高さの格子の登録上限に達しました\n");
		return HeightFieldHandle{};
	}

	HeightFieldData data{};
	data.countX = _countX;
	data.countZ = _countZ;
	data.cellSize = _cellSize;
	data.heights.assign(_heights.begin(), _heights.end());

	int index{ 0 };
	if (!heightFieldFreeList.empty())
	{
		index = heightFieldFreeList.top(); // Destroyされたところから持ってくる
		heightFieldFreeList.pop();
		heightFieldSlots[index].data = data;
		heightFieldSlots[index].isAlive = true; // 世代はDestroyで上げてあるのでそのまま
	}
	else
	{
		index = static_cast<int>(heightFieldSlots.size()); // 空きがないなら新しく作る
		heightFieldSlots.push_back({ data, 0, true }); // 新しく確保したので世代は0
	}
	const int pack{ Pack(index, static_cast<int>(heightFieldSlots[index].generation)) };
	return HeightFieldHandle{ PassKey{}, pack };
}

bool PhysicsSystem::DestroyHeightField(HeightFieldHandle _handle)
{
	HeightFieldData* data{ Lookup(_handle) };
	if (!data) return false;

	// Bodyが使っている間は断る
	for (const BodySlot& slot : slots)
	{
		if (!slot.isAlive) continue;
		if (slot.data.shape.index() != HEIGHT_FIELD_SHAPE_INDEX) continue;
		if (std::get<HeightFieldShape>(slot.data.shape).heightField == _handle)
		{
			DEBUG_LOG_ERROR("Bodyが使っている高さの格子は削除できません\n");
			return false;
		}
	}

	const int index{ UnpackIndex(_handle.GetRaw(PassKey{})) };
	heightFieldSlots[index].data = {};
	heightFieldSlots[index].generation++;
	heightFieldSlots[index].isAlive = false;
	heightFieldFreeList.push(index); // この位置を使えるようにする
	return true;
}

void PhysicsSystem::Step(float _deltaTime)
{
	// 刻む際の検査はループ外で一度だけ
	if(!(std::isfinite(_deltaTime) && _deltaTime > 0.0f))
	{
		DEBUG_LOG_ERROR("Stepに不正な時間が渡されました\n");
		return;
	}

	// 重力を速度へ足す(Dynamicだけ)
	for (BodySlot& slot : slots)
	{
		if (!slot.isAlive) continue;
		BodyData& data{ slot.data };
		if (data.motionType == MotionType::Dynamic && data.isGravityEnabled) data.velocity.y += GRAVITY_Y * _deltaTime;
	}

	// 今の位置で接触を調べる
	UpdateContacts();

	// 接触している組の速度を直す
	SolveVelocities();

	// 直した後の速度で位置を進める
	for (BodySlot& slot : slots)
	{
		if (!slot.isAlive) continue;
		BodyData& data{ slot.data };
		if (data.motionType == MotionType::Static) continue;
		data.position += data.velocity * _deltaTime;
	}

	// 残っためり込みを位置で戻す
	CorrectPositions();
}

Vector3 PhysicsSystem::GetPosition(BodyHandle _handle)
{
	const BodyData* data{ Lookup(_handle) };
	return data ? data->position : Vector3::Zero;
}

Quaternion PhysicsSystem::GetRotation(BodyHandle _handle)
{
	const BodyData* data{ Lookup(_handle) };
	return data ? data->rotation : Quaternion::Identity;
}

Vector3 PhysicsSystem::GetVelocity(BodyHandle _handle)
{
	const BodyData* data{ Lookup(_handle) };
	return data ? data->velocity : Vector3::Zero;
}

BodyState PhysicsSystem::GetState(BodyHandle _handle)
{
	const BodyData* data{ Lookup(_handle) }; // 3つ取るときも検証は1回
	if (!data) return BodyState{};
	return BodyState{ data->position, data->rotation, data->velocity };
}

bool PhysicsSystem::SetPosition(BodyHandle _handle, Vector3 _position)
{
	BodyData* data{ Lookup(_handle) };
	if (!data || !IsFinite(_position)) return false;
	data->position = _position;
	return true;
}

bool PhysicsSystem::SetRotation(BodyHandle _handle, Quaternion _rotation)
{
	BodyData* data{ Lookup(_handle) };
	if (!data || !IsValidRotation(_rotation)) return false;
	data->rotation = _rotation;
	return true;
}

bool PhysicsSystem::SetVelocity(BodyHandle _handle, Vector3 _velocity)
{
	BodyData* data{ Lookup(_handle) };
	if (!data || !IsFinite(_velocity)) return false;
	if (data->motionType == MotionType::Static) return false; // Staticは速度を持たない
	data->velocity = _velocity;
	return true;
}

std::span<const BodyToBodyResult> PhysicsSystem::GetContacts() const
{
	return contacts;
}

BodyData* PhysicsSystem::Lookup(BodyHandle _handle)
{
	if (!_handle.IsValid())
	{
		DEBUG_LOG_ERROR("無効なハンドルです\n");
		return nullptr;
	}
	const int packed{ _handle.GetRaw(PassKey{}) };
	const int index{ UnpackIndex(packed) };
	if (index < 0 || index >= static_cast<int>(slots.size()))
	{
		DEBUG_LOG_ERROR("ハンドルに範囲外のサイズが渡されました\n");
		return nullptr; 
	}

	BodySlot& slot{ slots[index] };
	if (UnpackGen(packed) != static_cast<int>(slot.generation))
	{
		DEBUG_LOG_WARNING("世代が異なります\n");
		return nullptr; // 世代チェック
	}
	if (!slot.isAlive)
	{
		DEBUG_LOG_WARNING("有効でないBodyです\n");
		return nullptr; // 生存チェック
	}
	return &slot.data;
}

TriangleMeshData* PhysicsSystem::Lookup(TriangleMeshHandle _handle)
{
	if (!_handle.IsValid())
	{
		DEBUG_LOG_ERROR("無効なハンドルです\n");
		return nullptr;
	}
	const int packed{ _handle.GetRaw(PassKey{}) };
	const int index{ UnpackIndex(packed) };
	if (index < 0 || index >= static_cast<int>(triangleMeshSlots.size()))
	{
		DEBUG_LOG_ERROR("ハンドルに範囲外のサイズが渡されました\n");
		return nullptr;
	}

	TriangleMeshSlot& slot{ triangleMeshSlots[index] };
	if (UnpackGen(packed) != static_cast<int>(slot.generation))
	{
		DEBUG_LOG_WARNING("世代が異なります\n");
		return nullptr; // 世代チェック
	}
	if (!slot.isAlive)
	{
		DEBUG_LOG_WARNING("有効でない三角形の集まりです\n");
		return nullptr; // 生存チェック
	}
	return &slot.data;
}

HeightFieldData* PhysicsSystem::Lookup(HeightFieldHandle _handle)
{
	if (!_handle.IsValid())
	{
		DEBUG_LOG_ERROR("無効なハンドルです\n");
		return nullptr;
	}
	const int packed{ _handle.GetRaw(PassKey{}) };
	const int index{ UnpackIndex(packed) };
	if (index < 0 || index >= static_cast<int>(heightFieldSlots.size()))
	{
		DEBUG_LOG_ERROR("ハンドルに範囲外のサイズが渡されました\n");
		return nullptr;
	}

	HeightFieldSlot& slot{ heightFieldSlots[index] };
	if (UnpackGen(packed) != static_cast<int>(slot.generation))
	{
		DEBUG_LOG_WARNING("世代が異なります\n");
		return nullptr; // 世代チェック
	}
	if (!slot.isAlive)
	{
		DEBUG_LOG_WARNING("有効でない高さの格子です\n");
		return nullptr; // 生存チェック
	}
	return &slot.data;
}

bool PhysicsSystem::ComputeBodyContact(const BodyData& _bodyA, const BodyData& _bodyB, ShapeToShapeResult& _result)
{
	const Shape shapeA{ ToWorldShape(_bodyA) };
	const Shape shapeB{ ToWorldShape(_bodyB) };
	return contactFunctionTable[shapeA.index()][shapeB.index()](collisionSystem, shapeA, shapeB, _result);
}

void PhysicsSystem::UpdateContacts()
{
	contacts.clear(); // 容量は残るので確保は起きない
	contactSolveData.clear();
	const int slotCount{ static_cast<int>(slots.size()) };
	for (int indexA = 0; indexA < slotCount; indexA++)
	{
		const BodySlot& slotA{ slots[indexA] };
		if (!slotA.isAlive) continue;

		// 同じ組を2回調べないようBは必ずAより後ろの番号から始める
		for (int indexB = indexA + 1; indexB < slotCount; indexB++)
		{
			const BodySlot& slotB{ slots[indexB] };
			if (!slotB.isAlive) continue;
			// どちらもDynamicでない組は押し合わないので調べない
			if (slotA.data.motionType != MotionType::Dynamic && slotB.data.motionType != MotionType::Dynamic) continue;
			// 上限に達したらこのStepはここまで
			if (static_cast<int>(contacts.size()) >= MAX_CONTACT_COUNT) return;

			// 高さの格子は表を通さず 形が重なる升の三角形を調べる
			if (slotA.data.shape.index() == HEIGHT_FIELD_SHAPE_INDEX || slotB.data.shape.index() == HEIGHT_FIELD_SHAPE_INDEX)
			{
				AddHeightFieldContacts(indexA, indexB);
				continue;
			}

			// 三角形の集まりは表を通さず三角形を一枚ずつ調べる
			if (slotA.data.shape.index() == TRIANGLE_MESH_SHAPE_INDEX || slotB.data.shape.index() == TRIANGLE_MESH_SHAPE_INDEX)
			{
				AddTriangleMeshContacts(indexA, indexB);
				continue;
			}

			BodyToBodyResult contact{};
			if (!ComputeBodyContact(slotA.data, slotB.data, contact.shapeToShapeResult)) continue;
			contact.bodyA = BodyHandle{ PassKey{}, Pack(indexA, static_cast<int>(slotA.generation)) };
			contact.bodyB = BodyHandle{ PassKey{}, Pack(indexB, static_cast<int>(slotB.generation)) };
			contacts.push_back(contact);

			ContactSolveData solve{};
			solve.indexA = indexA;
			solve.indexB = indexB;
			solve.inverseMassSum = slotA.data.massInverse + slotB.data.massInverse;
			solve.friction = std::sqrt(slotA.data.friction * slotB.data.friction);
			solve.isSolved = !slotA.data.isTrigger && !slotB.data.isTrigger && solve.inverseMassSum > 0.0f;
			MakeTangents(contact.shapeToShapeResult.normal, solve.tangent1, solve.tangent2);
			contactSolveData.push_back(solve);
		}
	}
}

void PhysicsSystem::AddTriangleMeshContacts(int _indexA, int _indexB)
{
	const BodyData& bodyA{ slots[_indexA].data };
	const BodyData& bodyB{ slots[_indexB].data };
	const bool isMeshA{ bodyA.shape.index() == TRIANGLE_MESH_SHAPE_INDEX };
	const bool isMeshB{ bodyB.shape.index() == TRIANGLE_MESH_SHAPE_INDEX };
	if (isMeshA && isMeshB) return; // 三角形の集まり同士は調べない

	const BodyData& meshBody{ isMeshA ? bodyA : bodyB };
	const BodyData& otherBody{ isMeshA ? bodyB : bodyA };

	// ハンドルから三角形の集まりを引く(CreateBodyで検証済みなのでStepの中ではログを出さずに調べる)
	const int packed{ std::get<TriangleMeshShape>(meshBody.shape).mesh.GetRaw(PassKey{}) };
	const int meshIndex{ UnpackIndex(packed) };
	if (meshIndex < 0 || meshIndex >= static_cast<int>(triangleMeshSlots.size())) return;
	const TriangleMeshSlot& meshSlot{ triangleMeshSlots[meshIndex] };
	if (!meshSlot.isAlive || UnpackGen(packed) != static_cast<int>(meshSlot.generation)) return;

	// 相手の形を三角形の集まりのBodyのローカルへ移す(三角形を毎Stepワールドへ直すより形1つを移す方が安い)
	const Shape localShape{ std::visit([&otherBody, &meshBody](const auto& _shape) { return ToMeshLocalShape(_shape, otherBody, meshBody); }, otherBody.shape) };

	for (const Triangle& triangle : meshSlot.data.triangles)
	{
		// 上限に達したらこのStepはここまで
		if (static_cast<int>(contacts.size()) >= MAX_CONTACT_COUNT) return;

		ShapeToShapeResult result{};
		CollisionSystem& system{ collisionSystem };
		const bool isHit{ std::visit([&system, &triangle, &result](const auto& _shape) { return ComputeTriangleContact(system, _shape, triangle, result); }, localShape) };
		if (!isHit) continue;

		// 結果をワールドへ戻す(法線は回転だけ 点は回転してから位置を足す)
		result.normal = meshBody.rotation.RotateVector(result.normal);
		for (int i = 0; i < result.pointCount; i++)
		{
			result.points[i].worldPosition = ToWorldPoint(result.points[i].worldPosition, meshBody);
		}
		// 計算した法線は相手の形->三角形なので三角形の集まりがA側なら反転してA->Bにそろえる
		if (isMeshA) result.normal = -result.normal;

		PushContact(_indexA, _indexB, result);
	}
}

void PhysicsSystem::AddHeightFieldContacts(int _indexA, int _indexB)
{
	const BodyData& bodyA{ slots[_indexA].data };
	const BodyData& bodyB{ slots[_indexB].data };
	const bool isFieldA{ bodyA.shape.index() == HEIGHT_FIELD_SHAPE_INDEX };
	const bool isFieldB{ bodyB.shape.index() == HEIGHT_FIELD_SHAPE_INDEX };
	if (isFieldA && isFieldB) return; // 高さの格子同士は調べない

	const BodyData& fieldBody{ isFieldA ? bodyA : bodyB };
	const BodyData& otherBody{ isFieldA ? bodyB : bodyA };
	if (otherBody.shape.index() == TRIANGLE_MESH_SHAPE_INDEX) return; // 動かない形同士は調べない

	// ハンドルから高さの格子を引く(CreateBodyで検証済みなのでStepの中ではログを出さずに調べる)
	const int packed{ std::get<HeightFieldShape>(fieldBody.shape).heightField.GetRaw(PassKey{}) };
	const int fieldIndex{ UnpackIndex(packed) };
	if (fieldIndex < 0 || fieldIndex >= static_cast<int>(heightFieldSlots.size())) return;
	const HeightFieldSlot& fieldSlot{ heightFieldSlots[fieldIndex] };
	if (!fieldSlot.isAlive || UnpackGen(packed) != static_cast<int>(fieldSlot.generation)) return;
	const HeightFieldData& field{ fieldSlot.data };

	// 相手の形を高さの格子のBodyのローカルへ移す(格子の角が原点になる)
	const Shape localShape{ std::visit([&otherBody, &fieldBody](const auto& _shape) { return ToMeshLocalShape(_shape, otherBody, fieldBody); }, otherBody.shape) };

	// 形がXとZに占める範囲から 重なる升の番号の範囲を出す 格子の外なら接触しない
	const ShapeRangeXZ range{ std::visit([](const auto& _shape) { return GetRangeXZ(_shape); }, localShape) };
	const float fieldSizeX{ static_cast<float>(field.countX - 1) * field.cellSize };
	const float fieldSizeZ{ static_cast<float>(field.countZ - 1) * field.cellSize };
	if (range.maxX < 0.0f || range.minX > fieldSizeX || range.maxZ < 0.0f || range.minZ > fieldSizeZ) return;

	// 位置を升の大きさで割って切り捨てると升の番号になる 端は最後の升へ収める
	const int cellMinX{ std::clamp(static_cast<int>(std::floor(range.minX / field.cellSize)), 0, field.countX - 2) };
	const int cellMaxX{ std::clamp(static_cast<int>(std::floor(range.maxX / field.cellSize)), 0, field.countX - 2) };
	const int cellMinZ{ std::clamp(static_cast<int>(std::floor(range.minZ / field.cellSize)), 0, field.countZ - 2) };
	const int cellMaxZ{ std::clamp(static_cast<int>(std::floor(range.maxZ / field.cellSize)), 0, field.countZ - 2) };

	for (int cellZ = cellMinZ; cellZ <= cellMaxZ; cellZ++)
	{
		for (int cellX = cellMinX; cellX <= cellMaxX; cellX++)
		{
			// 升の4つの角(名前の2つの数字は Xの番号とZの番号を 0=手前 1=次 で表す)
			const float x0{ static_cast<float>(cellX) * field.cellSize };
			const float x1{ static_cast<float>(cellX + 1) * field.cellSize };
			const float z0{ static_cast<float>(cellZ) * field.cellSize };
			const float z1{ static_cast<float>(cellZ + 1) * field.cellSize };
			const Vector3 corner00{ x0, field.heights[cellZ * field.countX + cellX], z0 };
			const Vector3 corner10{ x1, field.heights[cellZ * field.countX + cellX + 1], z0 };
			const Vector3 corner01{ x0, field.heights[(cellZ + 1) * field.countX + cellX], z1 };
			const Vector3 corner11{ x1, field.heights[(cellZ + 1) * field.countX + cellX + 1], z1 };

			// 升を corner00 と corner11 を結ぶ対角線で三角形2枚に割る(法線が上を向く順に並べる)
			const std::array<Triangle, 2> triangles{ Triangle{ corner00, corner01, corner11 }, Triangle{ corner00, corner11, corner10 } };
			for (const Triangle& triangle : triangles)
			{
				// 上限に達したらこのStepはここまで
				if (static_cast<int>(contacts.size()) >= MAX_CONTACT_COUNT) return;

				ShapeToShapeResult result{};
				CollisionSystem& system{ collisionSystem };
				const bool isHit{ std::visit([&system, &triangle, &result](const auto& _shape) { return ComputeTriangleContact(system, _shape, triangle, result); }, localShape) };
				if (!isHit) continue;

				// 結果をワールドへ戻す(法線は回転だけ 点は回転してから位置を足す)
				result.normal = fieldBody.rotation.RotateVector(result.normal);
				for (int i = 0; i < result.pointCount; i++)
				{
					result.points[i].worldPosition = ToWorldPoint(result.points[i].worldPosition, fieldBody);
				}
				// 計算した法線は相手の形->三角形なので高さの格子がA側なら反転してA->Bにそろえる
				if (isFieldA) result.normal = -result.normal;

				PushContact(_indexA, _indexB, result);
			}
		}
	}
}

void PhysicsSystem::PushContact(int _indexA, int _indexB, const ShapeToShapeResult& _result)
{
	const BodySlot& slotA{ slots[_indexA] };
	const BodySlot& slotB{ slots[_indexB] };

	BodyToBodyResult contact{};
	contact.shapeToShapeResult = _result;
	contact.bodyA = BodyHandle{ PassKey{}, Pack(_indexA, static_cast<int>(slotA.generation)) };
	contact.bodyB = BodyHandle{ PassKey{}, Pack(_indexB, static_cast<int>(slotB.generation)) };
	contacts.push_back(contact);

	ContactSolveData solve{};
	solve.indexA = _indexA;
	solve.indexB = _indexB;
	solve.inverseMassSum = slotA.data.massInverse + slotB.data.massInverse;
	solve.friction = std::sqrt(slotA.data.friction * slotB.data.friction);
	solve.isSolved = !slotA.data.isTrigger && !slotB.data.isTrigger && solve.inverseMassSum > 0.0f;
	MakeTangents(_result.normal, solve.tangent1, solve.tangent2);
	contactSolveData.push_back(solve);
}

void PhysicsSystem::SolveVelocities()
{
	const int contactCount{ static_cast<int>(contacts.size()) };
	for (int iteration = 0; iteration < SOLVER_ITERATION_COUNT; iteration++)
	{
		for (int i = 0; i < contactCount; i++)
		{
			ContactSolveData& solve{ contactSolveData[i] };
			if (!solve.isSolved) continue;

			BodyData& bodyA{ slots[solve.indexA].data };
			BodyData& bodyB{ slots[solve.indexB].data };
			const Vector3 normal{ contacts[i].shapeToShapeResult.normal };

			// 法線の向きの相対速度 負ならAとBが近づいている
			const float normalSpeed{ Vector3::Dot(bodyB.velocity - bodyA.velocity, normal) };
			// 近づく速さをちょうど0にする力積
			float normalImpulse{ -normalSpeed / solve.inverseMassSum };
			// 合計が負(引っ張り合う向き)にならないように切る
			const float previousNormalImpulse{ solve.normalImpulse };
			solve.normalImpulse = (std::max)(previousNormalImpulse + normalImpulse, 0.0f);
			normalImpulse = solve.normalImpulse - previousNormalImpulse;
			ApplyImpulse(bodyA, bodyB, normal * normalImpulse);

			// 摩擦 : 滑る速さを打ち消す ただし「摩擦の強さ × 押し付ける力積」まで
			const float maxFriction{ solve.friction * solve.normalImpulse };
			const auto solveTangent = [&](const Vector3& _tangent, float& _accumulated)
				{
					const float tangentSpeed{ Vector3::Dot(bodyB.velocity - bodyA.velocity, _tangent) };
					float tangentImpulse{ -tangentSpeed / solve.inverseMassSum };
					const float previous{ _accumulated };
					_accumulated = std::clamp(previous + tangentImpulse, -maxFriction, maxFriction);
					tangentImpulse = _accumulated - previous;
					ApplyImpulse(bodyA, bodyB, _tangent * tangentImpulse);
				};

			solveTangent(solve.tangent1, solve.tangentImpulse1);
			solveTangent(solve.tangent2, solve.tangentImpulse2);
		}
	}
}

void PhysicsSystem::CorrectPositions()
{
	const int contactCount{ static_cast<int>(contacts.size()) };
	for (int i = 0; i < contactCount; i++)
	{
		const ContactSolveData& solve{ contactSolveData[i] };
		if (!solve.isSolved) continue;
		const ShapeToShapeResult& result{ contacts[i].shapeToShapeResult };
		BodyData& bodyA{ slots[solve.indexA].data };
		BodyData& bodyB{ slots[solve.indexB].data };

		// 許す深さを超えた分だけ、質量の逆数の割合で2つに分けて戻す
		const float depth{ result.points[0].depth };
		const float correction{ (std::max)(depth - PENETRATION_SLOP, 0.0f) * PENETRATION_CORRECTION_RATE / solve.inverseMassSum };
		bodyA.position -= result.normal * (correction * bodyA.massInverse);
		bodyB.position += result.normal * (correction * bodyB.massInverse);
	}
}
