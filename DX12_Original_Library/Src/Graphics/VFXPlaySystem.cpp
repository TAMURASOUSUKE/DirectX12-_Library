#include <cstddef>
#include <cstring>
#include <utility>
#include <vector>
#include "../Debug/DebugLogs.h"
#include "../Core/Handle/HandlePacking.h"
#include "GraphicsResourceManager.h"
#include "VFXMeshSystem.h"
#include "VFXPlaySystem.h"

namespace
{
	constexpr std::uint32_t GENERATION_MASK{ 0x7FFF }; // 世代は15bit Packで上位へ置いても符号bitに届かない
}

bool VFXPlaySystem::Setup(VFXMeshSystem& _meshSystem)
{
	if (isInitialized)
	{
		DEBUG_LOG_WARNING("VFXPlaySystemはすでに初期化されています\n");
		return false;
	}

	meshSystem = &_meshSystem;
	isInitialized = true;
	return true;
}

void VFXPlaySystem::Shutdown()
{
	for (VFXPlaySlot& slot : slots)
	{
		ReleaseResources(slot.data);
	}

	slots.clear();
	particleTypes.clear();
	dispatchRequests.clear();
	meshSystem = nullptr;
	isInitialized = false;
}

VFXParticleTypeHandle VFXPlaySystem::RegisterParticle(const VFXParticleDesc& _desc)
{
	if (!isInitialized)
	{
		DEBUG_LOG_ERROR("VFXPlaySystemが初期化されません\n");
		return  VFXParticleTypeHandle{};
	}

	if (!_desc.material.IsValid() || !_desc.updatePipeline.IsValid() || !_desc.resetPipeline.IsValid())
	{
		DEBUG_LOG_ERROR("VFX粒子の種類のMaterialまたはPipelineが無効です\n");
		return VFXParticleTypeHandle{};
	}

	if (_desc.particleCount == 0 || _desc.maxInstances == 0 || _desc.duration <= 0.0f)
	{
		DEBUG_LOG_ERROR("VFX粒子の種類のparticleCount maxInstances durationが不正です\n");
		return VFXParticleTypeHandle{};
	}

	if (slots.size() + _desc.maxInstances > MAX_VFX_PLAY_SLOT_COUNT)
	{
		DEBUG_LOG_ERROR("VFXの再生枠が上限を超えます\n");
		return VFXParticleTypeHandle{};
	}

	GraphicsResourceManager& resourceManager{ GraphicsResourceManager::Instance() };

	// 粒子Bufferは全要素0で作る
	const std::vector<std::byte> zeroParticles(static_cast<std::size_t>(_desc.particleCount) * VFX_PARTICLE_STRIDE); // 個数 * 一つのサイズ

	const std::uint32_t zeroCounter{ 0 };
	GPUBufferDesc particleDesc{};
	particleDesc.elementCount = _desc.particleCount;
	particleDesc.elementStride = VFX_PARTICLE_STRIDE;
	particleDesc.allowUnorderedAccess = true;
	GPUBufferDesc counterDesc{};
	counterDesc.elementCount = 1; // カウンター1つ
	counterDesc.elementStride = sizeof(uint32_t);
	counterDesc.allowUnorderedAccess = true;

	VFXParticleTypeData particleType{};
	particleType.desc = _desc;
	particleType.slotIndices.reserve(_desc.maxInstances);
	const int particleTypeIndex{ static_cast<int>(particleTypes.size()) };

	for (std::uint32_t i = 0; i < _desc.maxInstances; i++)
	{
		VFXPlaySlot slot{};
		slot.data.particleTypeIndex = particleTypeIndex;
		slot.data.particleBuffer = resourceManager.CreateGPUBuffer(particleDesc, zeroParticles.data(), zeroParticles.size());
		slot.data.counterBuffer = resourceManager.CreateGPUBuffer(counterDesc, &zeroCounter, sizeof(zeroCounter));
		slot.data.material = resourceManager.CloneMaterial(_desc.material);

		const bool isCreated{ slot.data.particleBuffer.IsValid() && slot.data.counterBuffer.IsValid() && slot.data.material.IsValid() };
		if (!isCreated || !resourceManager.SetMaterialBuffer(slot.data.material, 0, slot.data.particleBuffer))
		{
			// 作りかけを全部捨てる(この種類の枠は末尾に並んでいる)
			ReleaseResources(slot.data);
			for (int createdIndex : particleType.slotIndices)
			{
				ReleaseResources(slots[createdIndex].data);
			}
			DEBUG_LOG_ERROR("VFXの再生枠の作成に失敗しました\n");
			return VFXParticleTypeHandle{};
		}
		particleType.slotIndices.push_back(static_cast<int>(slots.size()));
		slots.push_back(slot);
	}
	particleTypes.push_back(std::move(particleType));
	return VFXParticleTypeHandle{ PassKey{}, particleTypeIndex };
}

VFXHandle VFXPlaySystem::Play(VFXParticleTypeHandle _particleType, const Vector3& _position)
{
	if (!isInitialized) return VFXHandle{};

	if (!_particleType.IsValid())
	{
		DEBUG_LOG_ERROR("無効なVFXParticleTypeHandleです\n");
		return VFXHandle{};
	}
	const int particleTypeIndex{ _particleType.GetRaw(PassKey{}) };
	if (particleTypeIndex < 0 || particleTypeIndex >= static_cast<int>(particleTypes.size()))
	{
		DEBUG_LOG_ERROR("範囲外のVFXParticleTypeHandleです\n");
		return VFXHandle{};
	}

	// 空き枠を線形に探す
	for (int slotIndex : particleTypes[particleTypeIndex].slotIndices)
	{
		VFXPlaySlot& slot{ slots[slotIndex] };
		if (slot.data.isPlaying) continue;

		slot.data.isPlaying = true;
		slot.data.elapsed = 0.0f;
		slot.data.position = _position;
		slot.data.previousPosition = _position;
		return VFXHandle{ PassKey{}, Pack(slotIndex, static_cast<int>(slot.generation)) };
	}

	droppedPlayCount++;
	return VFXHandle{};
}

bool VFXPlaySystem::Update(VFXHandle _handle, float _deltaTime)
{
	VFXPlayData* data{ Lookup(_handle) };
	if (!data) return false;
	const VFXParticleDesc& desc{ particleTypes[data->particleTypeIndex].desc };

	data->elapsed += _deltaTime;

	// 終了 = 発生終了 + 粒子の最大寿命 GPUから読み戻さずに時間だけで判定する
	if (data->elapsed >= desc.duration + desc.parameter.lifeMax)
	{
		Release(_handle);
		return false;
	}

	// 発生中かつ時間が進んでいるときだけ発生させる(dt = 0 のヒットストップ中に粒子が積み上がらないように)
	const bool isEmitting{ data->elapsed < desc.duration && _deltaTime > 0.0f };
	Vector3 emitVelocity{ Vector3::Zero };
	if (_deltaTime > 0.0f) emitVelocity = (data->position - data->previousPosition) / _deltaTime;
	data->previousPosition = data->position;

	PushDispatchRequests(*data, desc, _deltaTime, isEmitting ? desc.emitPerFrame : 0u, emitVelocity);

	if (!meshSystem->RegisterInstanced(data->material, desc.particleCount, data->position)) DEBUG_LOG_ERROR("VFX粒子の描画登録に失敗しました\n");
	return true;
}

bool VFXPlaySystem::SetPosition(VFXHandle _handle, const Vector3& _position)
{
	VFXPlayData* data{ Lookup(_handle) };
	if (!data) return false;
	data->position = _position;
	return true;
}

bool VFXPlaySystem::IsFinished(VFXHandle _handle)
{
	return Lookup(_handle) == nullptr;
}

bool VFXPlaySystem::Stop(VFXHandle _handle, bool _immediate)
{
	VFXPlayData* data{ Lookup(_handle) };
	if (!data) return false;
	const VFXParticleDesc& desc{ particleTypes[data->particleTypeIndex].desc };

	if (!_immediate)
	{
		// 経過を寿命まで飛ばすと発生が止まり残った粒子は自然に消える
		if (data->elapsed < desc.duration) data->elapsed = desc.duration;
		return true;
	}

	// 即時 寿命の最大値より大きいdtで1回更新して全粒子のlifeを0以下にしてから枠を返す(描画登録はしない)
	PushDispatchRequests(*data, desc, desc.parameter.lifeMax + 1.0f, 0u, Vector3::Zero);
	Release(_handle);
	return true;
}

VFXPlayData* VFXPlaySystem::Lookup(VFXHandle _handle)
{
	if (!_handle.IsValid())
	{
		DEBUG_LOG_ERROR("無効なハンドルです\n");
		return nullptr; // 無効なハンドルならnull
	}

	int packed{ _handle.GetRaw(PassKey{}) }; // 生の値(内部ハンドルを取得)
	int index{ UnpackIndex(packed) }; // index部分を取り出す
	// 範囲外チェック
	if (index < 0 || index >= static_cast<int>(slots.size()))
	{
		DEBUG_LOG_ERROR("ハンドルに範囲外のサイズが渡されました\n");
		return nullptr; // 範囲チェック
	}
	VFXPlaySlot& slot{ slots[index] }; // スロットの指定ハンドル部分を取り出す
	// 世代チェック
	if (UnpackGen(packed) != static_cast<int>(slot.generation))
	{
		DEBUG_LOG_WARNING("世代が異なります\n");
		return nullptr; // 世代チェック
	}
	return &slot.data; // 実体を返す
}

void VFXPlaySystem::Release(VFXHandle _handle)
{
	VFXPlaySlot& slot{ slots[UnpackIndex(_handle.GetRaw(PassKey{}))] };
	slot.data.isPlaying = false;
	slot.generation = (slot.generation + 1) & GENERATION_MASK;
}

void VFXPlaySystem::ReleaseResources(VFXPlayData& _data)
{
	GraphicsResourceManager& resourceManager{ GraphicsResourceManager::Instance() };
	if (_data.material.IsValid()) resourceManager.Unload(_data.material);
	if (_data.counterBuffer.IsValid()) resourceManager.Unload(_data.counterBuffer);
	if (_data.particleBuffer.IsValid()) resourceManager.Unload(_data.particleBuffer);
	_data.material = MaterialHandle{};
	_data.counterBuffer = GPUBufferHandle{};
	_data.particleBuffer = GPUBufferHandle{};
}

void VFXPlaySystem::PushDispatchRequests(const VFXPlayData& _data, const VFXParticleDesc& _desc, float _deltaTime, uint32_t _emitCount, const Vector3& _emitVelocity)
{
	// カウンタを0へ(1スレッド)
	ComputeDispatchRequest reset{};
	reset.desc.pipeline = _desc.resetPipeline;
	reset.desc.outputs[0] = _data.counterBuffer; // u0
	reset.desc.groupCountX = 1;
	dispatchRequests.push_back(reset);

	// 更新と発生(スレッド = 粒子) 種類の値をコピーして毎フレーム変わる6つだけ書き換える
	frameSeed++;
	ParticleUpdateParameter parameter{ _desc.parameter };
	parameter.emitPosition[0] = _data.position.x;
	parameter.emitPosition[1] = _data.position.y;
	parameter.emitPosition[2] = _data.position.z;
	parameter.deltaTime = _deltaTime;
	parameter.emitCount = _emitCount;
	parameter.particleCount = _desc.particleCount;
	parameter.randomSeed = frameSeed;
	parameter.emitVelocity[0] = _emitVelocity.x;
	parameter.emitVelocity[1] = _emitVelocity.y;
	parameter.emitVelocity[2] = _emitVelocity.z;

	ComputeDispatchRequest update{};
	update.desc.pipeline = _desc.updatePipeline;
	update.desc.outputs[0] = _data.particleBuffer; // u0
	update.desc.outputs[1] = _data.counterBuffer; // u1
	update.desc.groupCountX = (_desc.particleCount + VFX_PARTICLE_THREAD_PER_GROUP - 1) / VFX_PARTICLE_THREAD_PER_GROUP; // 切り上げ
	static_assert(sizeof(parameter) <= MAX_COMPUTE_PARAMETER_SIZE, "ParticleUpdateParameterがCompute定数の上限を超えています");
	std::memcpy(update.parameter.data(), &parameter, sizeof(parameter));
	update.parameterSize = sizeof(parameter);
	dispatchRequests.push_back(update);
}
