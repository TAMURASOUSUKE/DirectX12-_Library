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
	contacts.reserve(MAX_CONTACT_COUNT); 
	contactSolveData.reserve(MAX_CONTACT_COUNT);
}

void PhysicsSystem::Shutdown()
{
	slots.clear();
	contacts.clear();
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
