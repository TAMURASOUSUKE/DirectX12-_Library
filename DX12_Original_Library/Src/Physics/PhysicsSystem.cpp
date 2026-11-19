#include <cmath>
#include <variant>
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
}

void PhysicsSystem::Setup()
{
	Shutdown(); // 再初期化対策
	slots.reserve(MAX_BODY_COUNT); // 最大容量をあらかじめ確保(作成時に再確保させない)
}

void PhysicsSystem::Shutdown()
{
	slots.clear();
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

	// 台帳を直接回す
	for (BodySlot& slot : slots)
	{
		if (!slot.isAlive) continue;
		BodyData& data{ slot.data };
		if (data.motionType == MotionType::Static) continue;

		// 先に速度へ重力を足す(Dynamicだけ)
		if (data.motionType == MotionType::Dynamic && data.isGravityEnabled) data.velocity.y += GRAVITY_Y * _deltaTime;

		// 足した後の速度で位置を進める
		data.position += data.velocity * _deltaTime;
	}
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
