#include <cmath>
#include <variant>
#include <array>
#include <algorithm>
#include "PhysicsContactDispatch.h"
#include "PhysicsShapeTransform.h"
#include "PhysicsSolver.h"
#include "PhysicsValidation.h"
#include "../Core/Handle/HandlePacking.h"
#include "../Debug/DebugLogs.h"
#include "PhysicsConstant.h"
#include "PhysicsSystem.h"

using  namespace::PhysicsContactDispatch;
using namespace::PhysicsShapeTransform;
using namespace::PhysicsSolver;
using namespace::PhysicsValidation;

namespace
{
	// 一つのレイヤーが一つのCollisionMaskに入っているかを返す
	bool IsLayerMatched(std::uint32_t _layer, std::uint32_t _collisionMask)
	{
		// 渡されたMaskの中に自分が入っているか
		return (_layer & _collisionMask) != 0;
	}

	// 組の並び順で左が先か(Aの番号 同じならBの番号で比べる)
	bool IsPairBefore(const ContactPair& _left, const ContactPair& _right)
	{
		if (_left.indexA != _right.indexA) return _left.indexA < _right.indexA;
		return _left.indexB < _right.indexB;
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

	// 最大容量をあらかじめ確保(作成時に再確保させない)
	slots.reserve(MAX_BODY_COUNT); 
	triangleMeshSlots.reserve(MAX_TRIANGLE_MESH_COUNT);
	heightFieldSlots.reserve(MAX_HEIGHT_FIELD_COUNT);
	contacts.reserve(MAX_CONTACT_COUNT);
	contactSolveData.reserve(MAX_CONTACT_COUNT);
	contactEvents.reserve(MAX_CONTACT_EVENT_COUNT);
	previousPairs.reserve(MAX_CONTACT_COUNT);
	currentPairs.reserve(MAX_CONTACT_COUNT);
}

void PhysicsSystem::Shutdown()
{
	slots.clear();
	contacts.clear();
	triangleMeshSlots.clear();
	contactEvents.clear();
	previousPairs.clear();
	currentPairs.clear();
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
	if (!(std::isfinite(_deltaTime) && _deltaTime > 0.0f))
	{
		DEBUG_LOG_ERROR("Stepに不正な時間が渡されました\n");
		return;
	}

	// 重力を速度へ足す(Dynamicだけ)
	for (BodySlot& slot : slots)
	{
		if (!slot.isAlive) continue;
		BodyData& data{ slot.data };
		if (data.motionType == MotionType::Dynamic && data.isGravityEnabled) data.velocity += gravity * _deltaTime;
	}

	// 今の位置で接触を調べる
	UpdateContacts();

	// 前のStepの組と比べて通知を溜める
	UpdateContactEvents();

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

			// Dynamicを含む組は押し合うので調べる トリガーを含む組は通知の溜めに調べる(両方staticの組は動かないので除く)
			const bool hasDynamic{ slotA.data.motionType == MotionType::Dynamic || slotB.data.motionType == MotionType::Dynamic };
			const bool hasTrigger{ slotA.data.isTrigger || slotB.data.isTrigger };
			const bool isBothStatic{ slotA.data.motionType == MotionType::Kinematic || slotB.data.motionType == MotionType::Kinematic };
			if (!hasDynamic && (!hasTrigger || isBothStatic)) continue;

			// 上限に達したらこのStepはここまで
			if (static_cast<int>(contacts.size()) >= MAX_CONTACT_COUNT) return;

			// データのレイヤーを見て双方向で判定する
			if (!IsLayerMatched(slotA.data.layer, slotB.data.collisionMask) || !IsLayerMatched(slotB.data.layer, slotA.data.collisionMask)) continue;

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

bool PhysicsSystem::Raycast(Vector3 _origin, Vector3 _direction, float _maxDistance, RaycastResult& _result, std::uint32_t _collisionMask, BodyHandle _ignoreBody)
{
	if (_maxDistance <= 0.0f)
	{
		DEBUG_LOG_ERROR("最大距離には0より大きい値を入れてください\n");
		return false;
	}

	if (_direction.LengthSquared() <= Math::EPSILON * Math::EPSILON)
	{
		DEBUG_LOG_ERROR("Directionの長さがほぼ0です\n");
		return false;
	}

	const Vector3 direction{ Vector3::Normalized(_direction) };
	bool isHit{ false }; // 今までのbodyでどれかに当たったか
	float nearestDistance{ _maxDistance };

	// 全Bodyを見る
	for (std::size_t slotIndex = 0; slotIndex < slots.size(); slotIndex++)
	{
		BodySlot& slot{ slots[slotIndex] };
		if (!slot.isAlive) continue;

		// 無視するBodyかをHandleを作ってチェック
		BodyHandle currentHandle{ PassKey{}, Pack(static_cast<int>(slotIndex), static_cast<int>(slot.generation)) }; // 世代とindexでハンドルを作る
		if (currentHandle == _ignoreBody) continue; // 無視するハンドルだった場合は弾く

		if (!IsLayerMatched(slot.data.layer, _collisionMask)) continue; // レイヤーに入っていなければ無視
		RayToShapeResult result{};
		bool isShapeHit{ false }; // このループの形状に当たったか
		// 各形状をget_ifで見て行う

		// 球
		if (const Sphere * sphere{ std::get_if<Sphere>(&slot.data.shape) })
		{
			const Sphere worldSphere{ ToWorldShape(*sphere, slot.data) };
			isShapeHit = collisionSystem.ComputeRayContact(_origin, direction, nearestDistance, worldSphere, result);
		}
		// 箱
		else if (const Box * box{ std::get_if<Box>(&slot.data.shape) })
		{
			const Box worldBox{ ToWorldShape(*box, slot.data) };
			isShapeHit = collisionSystem.ComputeRayContact(_origin, direction, nearestDistance, worldBox, result);
		}
		// カプセル
		else if (const Capsule * capsule{ std::get_if<Capsule>(&slot.data.shape) })
		{
			const Capsule worldCapsule{ ToWorldShape(*capsule, slot.data) };
			isShapeHit = collisionSystem.ComputeRayContact(_origin, direction, nearestDistance, worldCapsule, result);
		}
		// 三角形
		else if (const TriangleMeshShape * meshShape{ std::get_if<TriangleMeshShape>(&slot.data.shape) })
		{
			// ハンドルから三角形の集まりを引く(CreateBodyで検証済みなのでStepの中ではログを出さずに調べる)
			const int packed{ std::get<TriangleMeshShape>(slot.data.shape).mesh.GetRaw(PassKey{}) };
			const int meshIndex{ UnpackIndex(packed) };
			if (meshIndex < 0 || meshIndex >= static_cast<int>(triangleMeshSlots.size())) continue;
			const TriangleMeshSlot& meshSlot{ triangleMeshSlots[meshIndex] };
			if (!meshSlot.isAlive || UnpackGen(packed) != static_cast<int>(meshSlot.generation)) continue;

			// 三角形を全て動かすと重いのでRay側をBodyのローカルへ移す(向きは位置を引かず逆回転だけ)
			const Quaternion inverseRotation{ Vector4{ -slot.data.rotation.x, -slot.data.rotation.y, -slot.data.rotation.z, slot.data.rotation.w } };
			const Vector3 localOrigin{ ToLocalPoint(_origin, slot.data) };
			const Vector3 localDirection{ inverseRotation.RotateVector(direction) };

			float shapeNearestDistance{ nearestDistance }; // この形の中で一番近い距離
			for (const Triangle& triangle : meshSlot.data.triangles)
			{
				// resultは当たった時だけ書き換わるので使い回す
				if (!collisionSystem.ComputeRayContact(localOrigin, localDirection, shapeNearestDistance, triangle, result)) continue;
				shapeNearestDistance = result.distance;
				isShapeHit = true;
			}

			// 結果をワールドへ戻す(法線は回転だけ 点は回転してから位置を足す)
			if (isShapeHit)
			{
				result.normal = slot.data.rotation.RotateVector(result.normal);
				result.worldPosition = ToWorldPoint(result.worldPosition, slot.data);
			}
		}
		// 高さの格子
		else if (const HeightFieldShape * fieldShape{ std::get_if<HeightFieldShape>(&slot.data.shape) })
		{
			// ハンドルから高さの格子を引く
			const int packed{ fieldShape->heightField.GetRaw(PassKey{}) };
			const int fieldIndex{ UnpackIndex(packed) };
			if (fieldIndex < 0 || fieldIndex >= static_cast<int>(heightFieldSlots.size())) continue;
			const HeightFieldSlot& fieldSlot{ heightFieldSlots[fieldIndex] };
			if (!fieldSlot.isAlive || UnpackGen(packed) != static_cast<int>(fieldSlot.generation)) continue;
			const HeightFieldData& field{ fieldSlot.data };

			// Ray側をBodyのローカルへ移す格子の角を原点に移す
			const Quaternion inverseRotation{ Vector4{ -slot.data.rotation.x, -slot.data.rotation.y, -slot.data.rotation.z, slot.data.rotation.w } };
			const Vector3 localOrigin{ ToLocalPoint(_origin, slot.data) };
			const Vector3 localDirection{ inverseRotation.RotateVector(direction) };

			// Rayの始点と届く端がXとZに占める範囲を出す
			const Vector3 localEnd{ localOrigin + localDirection * nearestDistance };
			const float margin{ field.cellSize * 0.001f }; // 升の境目の真上を通るRayが丸めで隣の升へ落ちても拾えるよう少し広げる
			const ShapeRangeXZ range{
				(std::min)(localOrigin.x, localEnd.x) - margin, (std::max)(localOrigin.x, localEnd.x) + margin,
				(std::min)(localOrigin.z, localEnd.z) - margin, (std::max)(localOrigin.z, localEnd.z) + margin };
			const float fieldSizeX{ static_cast<float>(field.countX - 1) * field.cellSize };
			const float fieldSizeZ{ static_cast<float>(field.countZ - 1) * field.cellSize };
			if (range.maxX < 0.0f || range.minX > fieldSizeX || range.maxZ < 0.0f || range.minZ > fieldSizeZ) continue;

			// 位置を升の大きさで割って切り捨てると升の番号になる 端は最後の升へ収める
			const int cellMinX{ std::clamp(static_cast<int>(std::floor(range.minX / field.cellSize)), 0, field.countX - 2) };
			const int cellMaxX{ std::clamp(static_cast<int>(std::floor(range.maxX / field.cellSize)), 0, field.countX - 2) };
			const int cellMinZ{ std::clamp(static_cast<int>(std::floor(range.minZ / field.cellSize)), 0, field.countZ - 2) };
			const int cellMaxZ{ std::clamp(static_cast<int>(std::floor(range.maxZ / field.cellSize)), 0, field.countZ - 2) };

			float shapeNearestDistance{ nearestDistance }; // この形の中で一番近い距離
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
						if (!collisionSystem.ComputeRayContact(localOrigin, localDirection, shapeNearestDistance, triangle, result)) continue;
						shapeNearestDistance = result.distance;
						isShapeHit = true;
					}
				}
			}

			// 結果をワールドへ戻す(法線は回転だけ 点は回転してから位置を足す)
			if (isShapeHit)
			{
				result.normal = slot.data.rotation.RotateVector(result.normal);
				result.worldPosition = ToWorldPoint(result.worldPosition, slot.data);
			}
		}

		if (!isShapeHit) continue;
		_result.body = currentHandle;
		_result.distance = result.distance;
		_result.normal = result.normal;
		_result.worldPosition = result.worldPosition;
		_result.userData = slot.data.userData;
		nearestDistance = result.distance;
		isHit = true;
	}
	return isHit;
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

bool PhysicsSystem::Overlap(const Shape& _shape, OverlapResult& _result, std::uint32_t _collisionMask, BodyHandle _ignoreBody)
{
	_result.bodyCount = 0; // 前の呼び出しの中身を残さない

	// 調べる側の形は箱とカプセルと球
	const bool isTableShape{ _shape.index() != TRIANGLE_MESH_SHAPE_INDEX && _shape.index() != HEIGHT_FIELD_SHAPE_INDEX };
	// 入力された形状のIsValidを呼び出すvisit
	const bool isValidShape{ std::visit([](const auto& _queryShape) { return _queryShape.IsValid(); }, _shape) };
	if (!isTableShape || !isValidShape)
	{
		DEBUG_LOG_ERROR("Overlapに渡す形状が不正です\n");
		return false;
	}

	const BodyData identityBody{}; // 調べる形はワールドで渡されるので位置0回転なしにする
	CollisionSystem& system{ collisionSystem };

	// 全Bodyを見る
	for (std::size_t slotIndex = 0; slotIndex < slots.size(); slotIndex++)
	{
		if (_result.bodyCount >= MAX_OVERLAP_RESULT_COUNT) break;
		const BodySlot& slot{ slots[slotIndex] };
		if (!slot.isAlive) continue;

		const BodyHandle currentHandle{ PassKey{}, Pack(static_cast<int>(slotIndex), static_cast<int>(slot.generation)) }; // 世代とindexでハンドルを作る
		if (currentHandle == _ignoreBody) continue; // 無視するハンドルだった場合は弾く
		if (!IsLayerMatched(slot.data.layer, _collisionMask)) continue; // レイヤーに入っていなければ無視

		bool isOverlapped{ false }; // このBodyと重なっているか
		ShapeToShapeResult contactResult{}; // 関数が書き込む先

		// 高さの格子 形が重なるマスの三角形を調べる
		if (slot.data.shape.index() == HEIGHT_FIELD_SHAPE_INDEX)
		{
			// ハンドルから高さの格子を引く
			const int packed{ std::get<HeightFieldShape>(slot.data.shape).heightField.GetRaw(PassKey{}) };
			const int fieldIndex{ UnpackIndex(packed) };
			if (fieldIndex < 0 || fieldIndex >= static_cast<int>(heightFieldSlots.size())) continue;
			const HeightFieldSlot& fieldSlot{ heightFieldSlots[fieldIndex] };
			if (!fieldSlot.isAlive || UnpackGen(packed) != static_cast<int>(fieldSlot.generation)) continue;
			const HeightFieldData& field{ fieldSlot.data };

			// 調べる形を高さの格子のBodyのローカルへ移す
			const BodyData& fieldBody{ slot.data };
			const Shape localShape{ std::visit([&identityBody, &fieldBody](const auto& _queryShape) { return ToMeshLocalShape(_queryShape, identityBody, fieldBody); }, _shape) };

			// 形がXとZに占める範囲から重なるマスの番号の範囲をだす 格子の外なら重ならない
			const ShapeRangeXZ range{ std::visit([](const auto& _localShape) { return GetRangeXZ(_localShape); }, localShape) };
			const float fieldSizeX{ static_cast<float>(field.countX - 1) * field.cellSize };
			const float fieldSizeZ{ static_cast<float>(field.countZ - 1) * field.cellSize };
			if (range.maxX < 0.0f || range.minX > fieldSizeX || range.maxZ < 0.0f || range.minZ > fieldSizeZ) continue;

			// 位置をマスの大きさで割って切り捨てるとマスの番号になる端は最後のマスへ納める
			const int cellMinX{ std::clamp(static_cast<int>(std::floor(range.minX / field.cellSize)), 0, field.countX - 2) };
			const int cellMaxX{ std::clamp(static_cast<int>(std::floor(range.maxX / field.cellSize)), 0, field.countX - 2) };
			const int cellMinZ{ std::clamp(static_cast<int>(std::floor(range.minZ / field.cellSize)), 0, field.countZ - 2) };
			const int cellMaxZ{ std::clamp(static_cast<int>(std::floor(range.maxZ / field.cellSize)), 0, field.countZ - 2) };

			// 一枚でも重なっていればのこりは調べないので重なっていたら全てのループを抜ける
			for (int cellZ = cellMinZ; cellZ <= cellMaxZ && !isOverlapped; cellZ++)
			{
				for (int cellX = cellMinX; cellX <= cellMaxX && !isOverlapped; cellX++)
				{
					// マスの四つの角
					const float x0{ static_cast<float>(cellX) * field.cellSize };
					const float x1{ static_cast<float>(cellX + 1) * field.cellSize };
					const float z0{ static_cast<float>(cellZ) * field.cellSize };
					const float z1{ static_cast<float>(cellZ + 1) * field.cellSize };
					const Vector3 corner00{ x0, field.heights[cellZ * field.countX + cellX], z0 };
					const Vector3 corner10{ x1, field.heights[cellZ * field.countX + cellX + 1], z0 };
					const Vector3 corner01{ x0, field.heights[(cellZ + 1) * field.countX + cellX], z1 };
					const Vector3 corner11{ x1, field.heights[(cellZ + 1) * field.countX + cellX + 1], z1 };

					// マスをcorner00とcorner11を結ぶ対角線を引いて三角形2枚にする
					const std::array<Triangle, 2> triangles{ Triangle{ corner00, corner01, corner11 }, Triangle{ corner00, corner11, corner10 } };
					for (const Triangle& triangle : triangles)
					{
						isOverlapped = std::visit([&system, &triangle, &contactResult](const auto& _localShape) { return ComputeTriangleContact(system, _localShape, triangle, contactResult); }, localShape);
						if (isOverlapped) break;
					}
				}
			}
		}
		// 三角形の集まり
		else if (slot.data.shape.index() == TRIANGLE_MESH_SHAPE_INDEX)
		{
			// ハンドルから三角形の集まりを引く
			const int packed{ std::get<TriangleMeshShape>(slot.data.shape).mesh.GetRaw(PassKey{}) };
			const int meshIndex{ UnpackIndex(packed) };
			if (meshIndex < 0 || meshIndex >= static_cast<int>(triangleMeshSlots.size())) continue;
			const TriangleMeshSlot& meshSlot{ triangleMeshSlots[meshIndex] };
			if (!meshSlot.isAlive || UnpackGen(packed) != static_cast<int>(meshSlot.generation)) continue;

			// 調べる形を三角形の集まりのBodyのローカルへ移す
			const BodyData& meshBody{ slot.data };
			const Shape localShape{ std::visit([&identityBody, &meshBody](const auto& _queryShape) { return ToMeshLocalShape(_queryShape, identityBody, meshBody); }, _shape) };

			for (const Triangle& triangle : meshSlot.data.triangles)
			{
				isOverlapped = std::visit([&system, &triangle, &contactResult](const auto& _localShape) { return ComputeTriangleContact(system, _localShape, triangle, contactResult); }, localShape);
				if (isOverlapped) break; // 1枚でも重なっていれば残りは調べない
			}
		}
		// 箱とカプセルと球はワールドへ直してから表から関数を選ぶ
		else
		{
			const Shape bodyShape{ ToWorldShape(slot.data) };
			isOverlapped = contactFunctionTable[_shape.index()][bodyShape.index()](collisionSystem, _shape, bodyShape, contactResult);
		}
		if (!isOverlapped) continue;
		_result.bodies[_result.bodyCount] = currentHandle;
		_result.bodyCount++;
	}
	return _result.bodyCount > 0;
}

bool PhysicsSystem::Sweep(const Shape& _shape, Vector3 _direction, float _maxDistance, RaycastResult& _result, std::uint32_t _collisionMask, BodyHandle _ignoreBody)
{
	if (_maxDistance <= 0.0f)
	{
		DEBUG_LOG_ERROR("最大距離には0より大きい値を入れてください\n");
		return false;
	}

	if (_direction.LengthSquared() <= Math::EPSILON * Math::EPSILON)
	{
		DEBUG_LOG_ERROR("Directionの長さがほぼ0です\n");
		return false;
	}

	// 動かす形はカプセルと球だけ
	Capsule moving{};
	if (const Sphere * sphere{ std::get_if<Sphere>(&_shape) }) moving = Capsule{ sphere->center, sphere->center, sphere->radius }; // 球だったら長さ0のcapsule
	else if (const Capsule * capsule{ std::get_if<Capsule>(&_shape) }) moving = *capsule;
	else
	{
		DEBUG_LOG_ERROR("Sweepで動かせる形は球とカプセルです\n");
		return false;
	}

	if (!moving.IsValid())
	{
		DEBUG_LOG_ERROR("Sweepに渡す形状が不正です\n");
		return false;
	}

	const Vector3 direction{ Vector3::Normalized(_direction) };
	bool isHit{ false }; // 今までのbodyでどれかに当たったか
	float nearestDistance{ _maxDistance };

	for (std::size_t slotIndex = 0; slotIndex < slots.size(); slotIndex++)
	{
		BodySlot& slot{ slots[slotIndex] };
		if (!slot.isAlive) continue;

		BodyHandle currentHandle{ PassKey{}, Pack(static_cast<int>(slotIndex), static_cast<int>(slot.generation)) }; // 世代とindexでハンドルを作る
		if (currentHandle == _ignoreBody) continue; // 無視するハンドルだった場合は弾く

		if (!IsLayerMatched(slot.data.layer, _collisionMask)) continue; // レイヤーに入っていなければ無視
		RayToShapeResult result{};
		bool isShapeHit{ false }; // このループの形状に当たったか

		// 球
		if (const Sphere * sphere{ std::get_if<Sphere>(&slot.data.shape) })
		{
			const Sphere worldSphere{ ToWorldShape(*sphere, slot.data) };
			isShapeHit = collisionSystem.ComputeSweepContact(moving, direction, nearestDistance, worldSphere, result);
		}
		// 箱
		else if (const Box * box{ std::get_if<Box>(&slot.data.shape) })
		{
			const Box worldBox{ ToWorldShape(*box, slot.data) };
			isShapeHit = collisionSystem.ComputeSweepContact(moving, direction, nearestDistance, worldBox, result);
		}
		// カプセル
		else if (const Capsule * capsule{ std::get_if<Capsule>(&slot.data.shape) })
		{
			const Capsule worldCapsule{ ToWorldShape(*capsule, slot.data) };
			isShapeHit = collisionSystem.ComputeSweepContact(moving, direction, nearestDistance, worldCapsule, result);
		}
		// 三角形
		else if (const TriangleMeshShape * meshShape{ std::get_if<TriangleMeshShape>(&slot.data.shape) })
		{
			// ハンドルから三角形の集まりを引く
			const int packed{ meshShape->mesh.GetRaw(PassKey{}) };
			const int meshIndex{ UnpackIndex(packed) };
			if (meshIndex < 0 || meshIndex >= static_cast<int>(triangleMeshSlots.size())) continue;
			const TriangleMeshSlot& meshSlot{ triangleMeshSlots[meshIndex] };
			if (!meshSlot.isAlive || UnpackGen(packed) != static_cast<int>(meshSlot.generation)) continue;

			// 三角形を全て動かすと重いので動く形の側をBodyのローカルへ移す(向きは位置を引かず逆回転だけ)
			const Quaternion inverseRotation{ Vector4{ -slot.data.rotation.x, -slot.data.rotation.y, -slot.data.rotation.z, slot.data.rotation.w } };
			const Capsule localMoving{ ToLocalPoint(moving.startPos, slot.data), ToLocalPoint(moving.endPos, slot.data), moving.radius };
			const Vector3 localDirection{ inverseRotation.RotateVector(direction) };

			float shapeNearestDistance{ nearestDistance }; // この形の中で一番近い距離
			for (const Triangle& triangle : meshSlot.data.triangles)
			{
				// resultは当たった時だけ書き換わるので使い回す
				if (!collisionSystem.ComputeSweepContact(localMoving, localDirection, shapeNearestDistance, triangle, result)) continue;
				shapeNearestDistance = result.distance;
				isShapeHit = true;
			}

			// 結果をワールドへ戻す(法線は回転だけ 点は回転してから位置を足す)
			if (isShapeHit)
			{
				result.normal = slot.data.rotation.RotateVector(result.normal);
				result.worldPosition = ToWorldPoint(result.worldPosition, slot.data);
			}
		}
		// 高さの格子
		else if (const HeightFieldShape * fieldShape{ std::get_if<HeightFieldShape>(&slot.data.shape) })
		{
			// ハンドルから高さの格子を引く
			const int packed{ fieldShape->heightField.GetRaw(PassKey{}) };
			const int fieldIndex{ UnpackIndex(packed) };
			if (fieldIndex < 0 || fieldIndex >= static_cast<int>(heightFieldSlots.size())) continue;
			const HeightFieldSlot& fieldSlot{ heightFieldSlots[fieldIndex] };
			if (!fieldSlot.isAlive || UnpackGen(packed) != static_cast<int>(fieldSlot.generation)) continue;
			const HeightFieldData& field{ fieldSlot.data };

			// 動く形の側をBodyのローカルへ移す(格子の角が原点になる)
			const Quaternion inverseRotation{ Vector4{ -slot.data.rotation.x, -slot.data.rotation.y, -slot.data.rotation.z, slot.data.rotation.w } };
			const Capsule localMoving{ ToLocalPoint(moving.startPos, slot.data), ToLocalPoint(moving.endPos, slot.data), moving.radius };
			const Vector3 localDirection{ inverseRotation.RotateVector(direction) };

			// 動く前の形の範囲と 届く端まで動いた後の範囲を合わせる
			const ShapeRangeXZ startRange{ GetRangeXZ(localMoving) };
			const Vector3 localMove{ localDirection * nearestDistance };
			const ShapeRangeXZ range{
				(std::min)(startRange.minX, startRange.minX + localMove.x), (std::max)(startRange.maxX, startRange.maxX + localMove.x),
				(std::min)(startRange.minZ, startRange.minZ + localMove.z), (std::max)(startRange.maxZ, startRange.maxZ + localMove.z) };
			const float fieldSizeX{ static_cast<float>(field.countX - 1) * field.cellSize };
			const float fieldSizeZ{ static_cast<float>(field.countZ - 1) * field.cellSize };
			if (range.maxX < 0.0f || range.minX > fieldSizeX || range.maxZ < 0.0f || range.minZ > fieldSizeZ) continue;

			// 位置を升の大きさで割って切り捨てると升の番号になる 端は最後の升へ収める
			const int cellMinX{ std::clamp(static_cast<int>(std::floor(range.minX / field.cellSize)), 0, field.countX - 2) };
			const int cellMaxX{ std::clamp(static_cast<int>(std::floor(range.maxX / field.cellSize)), 0, field.countX - 2) };
			const int cellMinZ{ std::clamp(static_cast<int>(std::floor(range.minZ / field.cellSize)), 0, field.countZ - 2) };
			const int cellMaxZ{ std::clamp(static_cast<int>(std::floor(range.maxZ / field.cellSize)), 0, field.countZ - 2) };

			float shapeNearestDistance{ nearestDistance }; // この形の中で一番近い距離
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
						if (!collisionSystem.ComputeSweepContact(localMoving, localDirection, shapeNearestDistance, triangle, result)) continue;
						shapeNearestDistance = result.distance;
						isShapeHit = true;
					}
				}
			}

			// 結果をワールドへ戻す(法線は回転だけ 点は回転してから位置を足す)
			if (isShapeHit)
			{
				result.normal = slot.data.rotation.RotateVector(result.normal);
				result.worldPosition = ToWorldPoint(result.worldPosition, slot.data);
			}
		}
		if (!isShapeHit) continue;
		_result.body = currentHandle;
		_result.distance = result.distance;
		_result.normal = result.normal;
		_result.worldPosition = result.worldPosition;
		_result.userData = slot.data.userData;
		nearestDistance = result.distance;
		isHit = true;
	}
	return isHit;
}

bool PhysicsSystem::MoveCharacter(const Capsule& _capsule, Vector3 _move, const CharacterMoveSetting& _setting, CharacterMoveResult& _result)
{
	_result = CharacterMoveResult{};
	if (!_capsule.IsValid())
	{
		DEBUG_LOG_ERROR("MoveCharacterに渡す形状が不正です\n");
		return false;
	}
	if (_setting.skinWidth < 0.0f || _setting.groundProbeDistance <= 0.0f || _setting.upDirection.LengthSquared() <= Math::EPSILON * Math::EPSILON)
	{
		DEBUG_LOG_ERROR("MoveCharacterに渡す設定が不正です\n");
		return false;
	}

	const Vector3 up{ Vector3::Normalized(_setting.upDirection) };
	const float groundNormalUp{ std::cos(_setting.slopeLimit) }; // 地面とみなす法線が上へ向く量の下限
	Vector3 moved{ Vector3::Zero }; // ここまで動いた量
	Vector3 remaining{ _move }; // まだ動いていない量
	Vector3 previousNormal{ Vector3::Zero }; // 一つ前に沿わせた面の法線
	bool hasPreviousNormal{ false }; // 一つ前に沿わせた面があるか

	for (int i = 0; i < CHARACTER_SLIDE_COUNT; i++)
	{
		const float remainingLength{ remaining.Length() };
		if (remainingLength <= Math::EPSILON) break; // 残りの量がほぼ0になったら抜ける
		const Vector3 direction{ remaining / remainingLength };

		// 隙間の分だけ太らせた形で調べる ここで触れた位置で止めると本体は隙間を保ったまま止まる
		const Capsule inflated{ _capsule.startPos + moved, _capsule.endPos + moved, _capsule.radius + _setting.skinWidth };
		RaycastResult hit{};
		if (!Sweep(inflated, direction, remainingLength, hit, _setting.collisionMask, _setting.ignoreBody))
		{
			// 何にも当たらなければすすめる
			moved += remaining;
			break;
		}

		// 当たった距離まで進む
		moved += direction * hit.distance;
		Vector3 rest{ direction * (remainingLength - hit.distance) }; // 当たった後に残った動き
		const float normalUp{ Vector3::Dot(hit.normal, up) }; // 法線が上へ向いている量

		// 下を向いた面に当たったことを覚える
		if (normalUp <= -groundNormalUp) _result.isCeilingHit = true;

		// 歩ける地面は下向きの分を支える(坂などに立っている時のずり落ち防止)
		if (normalUp >= groundNormalUp)
		{
			const float restUp{ Vector3::Dot(rest, up) }; // 残りが上に向いている量
			if (restUp < 0.0f) rest -= up * restUp;
		}

		// 残りから面へ向かう成分を引いて面に沿う成分だけ残す
		Vector3 slideNormal{ hit.normal }; // 沿わせる面の法線
		remaining = rest - slideNormal * Vector3::Dot(rest, slideNormal);

		// 急な坂を登る向きになったら法線を水平に倒して壁として扱う
		if (_setting.isSlopeLimitEnabled && normalUp > 0.0f && normalUp < groundNormalUp && Vector3::Dot(remaining, up) > 0.0f)
		{
			const Vector3 flatNormal{ hit.normal - up * normalUp }; // 上へ向いている分を引くと水平の成分だけ残る
			const float flatLength{ flatNormal.Length() };
			if (flatLength > Math::EPSILON)
			{
				slideNormal = flatNormal / flatLength;
				remaining = rest - slideNormal * Vector3::Dot(rest, slideNormal);
			}
		}

		// 残りが一つ前の面へ入る向きなら2つめの面の境界の線に沿う部分だけ残す
		if (hasPreviousNormal && Vector3::Dot(remaining, previousNormal) < 0.0f)
		{
			const Vector3 crease{ Vector3::Cross(previousNormal, slideNormal) }; // 二つの境界の境目の向き
			const float creaseLengthSquared{ crease.LengthSquared() };
			if (creaseLengthSquared <= Math::EPSILON * Math::EPSILON) break; // 二つの面が同じか真逆で面を作れない
			remaining = crease * (Vector3::Dot(remaining, crease) / creaseLengthSquared);
		}
		previousNormal = slideNormal;
		hasPreviousNormal = true;
	}

	// 下へ行こうとしている時はこのフレームの横の移動で足元が下がる分まで調べる距離を伸ばす
	float probeDistance{ _setting.groundProbeDistance };
	if (Vector3::Dot(_move, up) < 0.0f && groundNormalUp > Math::EPSILON)
	{
		const Vector3 lateral{ moved - up * Vector3::Dot(moved, up) }; // 動いた量から上下を引いた横の移動
		const float neededDistance{ lateral.Length() * std::tan(_setting.slopeLimit) + _setting.skinWidth / groundNormalUp }; // 登れる一番急な坂で足元が下がる量
		probeDistance = (std::max)(probeDistance, (std::min)(neededDistance, _setting.maxGroundProbeDistance));
	}

	// 動いたあとの位置から下へ短く調べる
	const Capsule movedCapsule{ _capsule.startPos + moved, _capsule.endPos + moved, _capsule.radius };
	RaycastResult ground{};
	if (Sweep(movedCapsule, -up, probeDistance, ground, _setting.collisionMask, _setting.ignoreBody) && Vector3::Dot(ground.normal, up) >= groundNormalUp)
	{
		_result.isGrounded = true;
		_result.groundNormal = ground.normal;
		// 下へ動こうとしている時は地面との間を隙間の分まで詰める
		if (Vector3::Dot(_move, up) < 0.0f && ground.distance > _setting.skinWidth) moved -= up * (ground.distance - _setting.skinWidth);
	}
	_result.movement = moved;
	return true;
}

std::span<const ContactEvent> PhysicsSystem::GetContactEvents() const
{
	return contactEvents;
}

void PhysicsSystem::ClearContactEvents()
{
	contactEvents.clear();
}

void PhysicsSystem::SetContactStayEventEnabled(bool _isEnabled)
{
	isContactStayEventEnabled = _isEnabled;
}

void PhysicsSystem::SetGravity(Vector3 _gravity)
{
	if (!std::isfinite(_gravity.x) || !std::isfinite(_gravity.y) || !std::isfinite(_gravity.z))
	{
		DEBUG_LOG_ERROR("SetGravityに不正な値が渡されました\n");
		return;
	}

	gravity = _gravity;
}

Vector3 PhysicsSystem::GetGravity() const
{
	return gravity;
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

void PhysicsSystem::UpdateContactEvents()
{
	// 今のStepの組を前のStepの組として残す
	previousPairs = currentPairs;
	currentPairs.clear();

	// contactSolverDataは組の小さい番号順に並び 同じ組は隣合うので直前と同じ組は足さない
	for (const ContactSolveData& solve : contactSolveData)
	{
		if (!currentPairs.empty() && currentPairs.back().indexA == solve.indexA && currentPairs.back().indexB == solve.indexB) continue;

		const BodySlot& slotA{ slots[solve.indexA] };
		const BodySlot& slotB{ slots[solve.indexB] };
		ContactPair pair{};
		pair.indexA = solve.indexA;
		pair.indexB = solve.indexB;
		pair.generationA = slotA.generation;
		pair.generationB = slotB.generation;
		pair.userDataA = slotA.data.userData;
		pair.userDataB = slotB.data.userData;
		currentPairs.push_back(pair);
	}

	// 2つの一覧を先頭から順に比べる
	std::size_t previousIndex{ 0 };
	std::size_t currentIndex{ 0 };
	while (previousIndex < previousPairs.size() || currentIndex < currentPairs.size())
	{
		// 相手の一覧が終わっているか自分の組の方が先ならその組は自分の一覧にしかない
		const bool isPreviousOnly{ currentIndex >= currentPairs.size() || (previousIndex < previousPairs.size() && IsPairBefore(previousPairs[previousIndex], currentPairs[currentIndex])) };
		const bool isCurrentOnly{ previousIndex >= previousPairs.size() || (currentIndex < currentPairs.size() && IsPairBefore(currentPairs[currentIndex], previousPairs[previousIndex])) };
		
		// 前のStepにしかない組は離れた
		if (isPreviousOnly)
		{
			PushContactEvent(ContactEventType::End, previousPairs[previousIndex]);
			previousIndex++;
			continue;
		}

		// 今のStepにしかない組は触れ始めた
		if (isCurrentOnly)
		{
			PushContactEvent(ContactEventType::Begin, currentPairs[currentIndex]);
			currentIndex++;
			continue;
		}

		// 両方にある組 世代が同じなら触れ続けている
		const ContactPair& previous{ previousPairs[previousIndex] };
		const ContactPair& current{ currentPairs[currentIndex] };
		if (previous.generationA == current.generationA && previous.generationB == current.generationB)
		{
			if (isContactStayEventEnabled) PushContactEvent(ContactEventType::Stay, current);
		}
		else
		{
			// 番号が同じでも世代が違えば別のBodyなので 離れたと触れ始めたの両方を出す
			PushContactEvent(ContactEventType::End, previous);
			PushContactEvent(ContactEventType::Begin, current);
		}
		previousIndex++;
		currentIndex++;
	}

	if (static_cast<int>(contactEvents.size()) >= MAX_CONTACT_EVENT_COUNT) DEBUG_LOG_ERROR("接触の通知が上限に達しました ClearContactEventsを呼んでください\n");
}

void PhysicsSystem::PushContactEvent(ContactEventType _type, const ContactPair& _pair)
{
	// 上限に達したら足さない
	if (static_cast<int>(contactEvents.size()) >= MAX_CONTACT_EVENT_COUNT) return;
	ContactEvent contactEvent{};
	contactEvent.type = _type;
	contactEvent.bodyA = BodyHandle{ PassKey{}, Pack(_pair.indexA, static_cast<int>(_pair.generationA)) };
	contactEvent.bodyB = BodyHandle{ PassKey{}, Pack(_pair.indexB, static_cast<int>(_pair.generationB)) };
	contactEvent.userDataA = _pair.userDataA;
	contactEvent.userDataB = _pair.userDataB;
	contactEvents.push_back(contactEvent);
}
