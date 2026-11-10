#include <cstddef>
#include <cstring>
#include <utility>
#include <vector>
#include <algorithm>
#include <array>
#include "CameraSystem.h"
#include "GraphicsConstant.h"
#include "../Debug/DebugLogs.h"
#include "../Core/Handle/HandlePacking.h"
#include "GraphicsResourceManager.h"
#include "VFXMeshSystem.h"
#include "VFXPlaySystem.h"

namespace
{
	constexpr std::uint32_t GENERATION_MASK{ 0x7FFF }; // 世代は15bit Packで上位へ置いても符号bitに届かない
}

bool VFXPlaySystem::Setup(VFXMeshSystem& _meshSystem, const CameraSystem& _cameraSystem)
{
	if (isInitialized)
	{
		DEBUG_LOG_WARNING("VFXPlaySystemはすでに初期化されています\n");
		return false;
	}

	meshSystem = &_meshSystem;
	cameraSystem = &_cameraSystem;
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
	meshTypes.clear();
	trailPoints.clear();
	meshSystem = nullptr;
	cameraSystem = nullptr;
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

VFXMeshTypeHandle VFXPlaySystem::RegisterMesh(const VFXMeshDesc& _desc)
{
	if (!isInitialized)
	{
		DEBUG_LOG_ERROR("VFXPlaySystemが初期化されていません\n");
		return VFXMeshTypeHandle{};
	}

	if (!_desc.material.IsValid() || _desc.maxInstances == 0 || _desc.duration <= 0.0f)
	{
		DEBUG_LOG_ERROR("VFXメッシュの種類のmaterial maxInstances durationが不正です\n");
		return VFXMeshTypeHandle{};
	}

	const bool isTrail{ _desc.kind == VFXMeshKind::Trail };
	if (isTrail && (_desc.trailPointCount < 2 || _desc.trailPointCount * 2 > MAX_VFX_VERTEX_COUNT || _desc.trailPointLife <= 0.0f))
	{
		DEBUG_LOG_ERROR("VFXの帯のtrailPointCount trailPointLifeが不正です\n");
		return VFXMeshTypeHandle{};
	}

	if (slots.size() + _desc.maxInstances > MAX_VFX_PLAY_SLOT_COUNT)
	{
		DEBUG_LOG_ERROR("VFXの再生枠が上限を超えます\n");
		return VFXMeshTypeHandle{};
	}

	GraphicsResourceManager& resourceManager{ GraphicsResourceManager::Instance() };

	VFXMeshTypeData meshType{};
	meshType.desc = _desc;
	meshType.slotIndices.reserve(_desc.maxInstances);
	const int meshTypeIndex{ static_cast<int>(meshTypes.size()) };

	for (std::uint32_t i = 0; i < _desc.maxInstances; i++)
	{
		VFXPlaySlot slot{};
		slot.data.meshTypeIndex = meshTypeIndex;
		slot.data.material = resourceManager.CloneMaterial(_desc.material);
		if (!slot.data.material.IsValid())
		{
			// 作りかけを全部捨てる
			for (int createdIndex : meshType.slotIndices)
			{
				ReleaseResources(slots[createdIndex].data);
			}
			DEBUG_LOG_ERROR("VFXメッシュの再生枠の作成に失敗しました\n");
			return VFXMeshTypeHandle{};
		}
		if (isTrail)
		{
			// この枠の点の区間を全体の配列の末尾に割り当てる
			slot.data.trailStart = static_cast<int>(trailPoints.size());
			trailPoints.resize(trailPoints.size() + _desc.trailPointCount);
		}
		meshType.slotIndices.push_back(static_cast<int>(slots.size()));
		slots.push_back(slot);
	}

	// 帯の頂点の使い回し領域を一番長い帯に合わせておく
	if (isTrail) trailVertexScratch.reserve(static_cast<std::size_t>(_desc.trailPointCount) * 2);

	meshTypes.push_back(std::move(meshType));
	return VFXMeshTypeHandle{ PassKey{}, meshTypeIndex };
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

VFXHandle VFXPlaySystem::Play(VFXMeshTypeHandle _meshType, const Vector3& _position, const Quaternion& _rotation)
{
	if (!isInitialized) return VFXHandle{};

	if (!_meshType.IsValid())
	{
		DEBUG_LOG_ERROR("無効なVFXMeshTypeHandleです\n");
		return VFXHandle{};
	}
	const int meshTypeIndex{ _meshType.GetRaw(PassKey{}) };
	if (meshTypeIndex < 0 || meshTypeIndex >= static_cast<int>(meshTypes.size()))
	{
		DEBUG_LOG_ERROR("範囲外のVFXMeshTypeHandleです\n");
		return VFXHandle{};
	}

	// 空き枠を線形に探す
	for (int slotIndex : meshTypes[meshTypeIndex].slotIndices)
	{
		VFXPlaySlot& slot{ slots[slotIndex] };
		if (slot.data.isPlaying) continue;

		slot.data.isPlaying = true;
		slot.data.elapsed = 0.0f;
		slot.data.position = _position;
		slot.data.previousPosition = _position;
		slot.data.rotation = _rotation;
		slot.data.trailHead = 0;
		slot.data.trailCount = 0;
		return VFXHandle{ PassKey{}, Pack(slotIndex, static_cast<int>(slot.generation)) };
	}

	droppedPlayCount++;
	return VFXHandle{};
}

bool VFXPlaySystem::Update(VFXHandle _handle, float _deltaTime)
{
	VFXPlayData* data{ Lookup(_handle) };
	if (!data) return false;

	// メッシュ(板 帯)の枠は粒子の台帳を読む前に分ける
	if (data->meshTypeIndex >= 0)
	{
		const VFXMeshDesc& meshDesc{ meshTypes[data->meshTypeIndex].desc };
		data->elapsed += _deltaTime;

		// ここで種類で分ける
		if (meshDesc.kind == VFXMeshKind::Trail) return UpdateTrail(_handle, *data, meshDesc, _deltaTime);
		return UpdateQuad(_handle, *data, meshDesc);
	}

	const VFXParticleDesc& desc{ particleTypes[data->particleTypeIndex].desc };

	data->elapsed += _deltaTime;

	// 終了 = 発生終了 + 粒子の最大寿命 GPUから読み戻さずに時間だけで判定する
	if (data->elapsed >= desc.duration + desc.parameter.lifeMax)
	{
		Release(_handle);
		return false;
	}

	// 再生全体(発生 + 余韻)に対する進み具合をMaterialへ渡す
	WriteTimeParameter(data->material, data->elapsed, desc.duration + desc.parameter.lifeMax);

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

	if (data->meshTypeIndex >= 0)
	{
		const VFXMeshDesc& meshDesc{ meshTypes[data->meshTypeIndex].desc };
		// 帯の既定は点の受け付けだけ止めて残りが寿命で消えるのを待つ
		if (meshDesc.kind == VFXMeshKind::Trail && !_immediate)
		{
			if (data->elapsed < meshDesc.duration) data->elapsed = meshDesc.duration;
			return true;
		}
		// 板は余韻が無いのでどちらの止め方でもすぐ枠を返す
		Release(_handle);
		return true;
	}

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

bool VFXPlaySystem::SetTrailPoint(VFXHandle _handle, const Vector3& _left, const Vector3& _right)
{
	VFXPlayData* data{ Lookup(_handle) };
	if (!data) return false;

	if (data->meshTypeIndex < 0) return false;

	const VFXMeshDesc& desc{ meshTypes[data->meshTypeIndex].desc };
	if (desc.kind != VFXMeshKind::Trail) return false;

	// 受け付けの秒数を過ぎていたら足さない
	if (data->elapsed >= desc.duration) return false;

	const int capacity{ static_cast<int>(desc.trailPointCount) };
	VFXTrailPoint* points{ trailPoints.data() + data->trailStart };
	if (data->trailCount < capacity)
	{
		// 空きがあるので一番新しい位置へ足す
		points[(data->trailHead + data->trailCount) % capacity] = VFXTrailPoint{ _left, _right, 0.0f };
		data->trailCount++;
	}
	else
	{
		// いっぱいなら一番古い点に上書きして先頭を1つ進める
		points[data->trailHead] = VFXTrailPoint{ _left, _right, 0.0f };
		data->trailHead = (data->trailHead + 1) % capacity;
	}
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

bool VFXPlaySystem::UpdateQuad(VFXHandle _handle, VFXPlayData& _data, const VFXMeshDesc& _desc)
{
	// 終了 = 再生する秒数に達した
	if (_data.elapsed >= _desc.duration)
	{
		Release(_handle);
		return false;
	}

	WriteTimeParameter(_data.material, _data.elapsed, _desc.duration);

	// 板の右と上をモードから決める
	Vector3 right{ Vector3::Right };
	Vector3 up{ Vector3::Up };
	if (!VFXMeshSystem::ComputeQuadAxes(_desc.billboardMode, _data.rotation, _data.position, cameraSystem->GetCameraRotation(), cameraSystem->GetCameraPosition(), right, up))
	{
		DEBUG_LOG_ERROR("不明なBillboardModeが設定されています\n");
		return true;
	}

	const Vector3 halfRight{ right * (_desc.width * 0.5f) };
	const Vector3 halfUp{ up * (_desc.height * 0.5f) };
	const Vector4 white{ 1.0f, 1.0f, 1.0f, 1.0f }; // 色と強さはMaterialが決めるので白

	// TRIANGLESTRIPの並び 左上 右上 左下 右下
	const std::array<VFXVertex, 4> vertices
	{
		VFXMeshSystem::MakeVertex(_data.position - halfRight + halfUp, 0.0f, 0.0f, white),
		VFXMeshSystem::MakeVertex(_data.position + halfRight + halfUp, 1.0f, 0.0f, white),
		VFXMeshSystem::MakeVertex(_data.position - halfRight - halfUp, 0.0f, 1.0f, white),
		VFXMeshSystem::MakeVertex(_data.position + halfRight - halfUp, 1.0f, 1.0f, white),
	};
	if (!meshSystem->Register(vertices, _data.material)) DEBUG_LOG_ERROR("VFXの板の描画登録に失敗しました\n");
	return true;
}

bool VFXPlaySystem::UpdateTrail(VFXHandle _handle, VFXPlayData& _data, const VFXMeshDesc& _desc, float _deltaTime)
{
	const int capacity{ static_cast<int>(_desc.trailPointCount) };
	VFXTrailPoint* points{ trailPoints.data() + _data.trailStart };

	// 全部の点の秒数を進める
	for (int i = 0; i < _data.trailCount; i++)
	{
		points[(_data.trailHead + i) % capacity].age += _deltaTime;
	}

	// 寿命を過ぎた古い点を先頭から捨てる
	while (_data.trailCount > 0 && points[_data.trailHead].age >= _desc.trailPointLife)
	{
		_data.trailHead = (_data.trailHead + 1) % capacity; // 古い点をずらす
		_data.trailCount--; // 全体の数を減らす
	}

	// 終了 = 受け付けが終わり 残りの点が帯にならない数まで減った
	if (_data.elapsed >= _desc.duration && _data.trailCount < 2)
	{
		Release(_handle);
		return false;
	}

	WriteTimeParameter(_data.material, _data.elapsed, _desc.duration);

	// 2点無いと帯にならないので描かない
	if (_data.trailCount < 2) return true;

	// 古い順に左右の2頂点を並べる 長さ方向のUVは古い端が0 新しい端が1
	std::vector<VFXVertex>& vertices{ trailVertexScratch };
	vertices.clear();
	const float lengthStep{ 1.0f / static_cast<float>(_data.trailCount - 1) };
	for (int i = 0; i < _data.trailCount; i++)
	{
		const VFXTrailPoint& point{ points[(_data.trailHead + i) % capacity] };
		const float u{ lengthStep * static_cast<float>(i) };
		// 頂点色のαは点の新しさ 渡した直後が1 寿命で0
		const Vector4 color{ 1.0f, 1.0f, 1.0f, 1.0f - point.age / _desc.trailPointLife };
		vertices.push_back(VFXMeshSystem::MakeVertex(point.left, u, 0.0f, color));
		vertices.push_back(VFXMeshSystem::MakeVertex(point.right, u, 1.0f, color));
	}
	if (!meshSystem->Register(vertices, _data.material)) DEBUG_LOG_ERROR("VFXの帯の描画登録に失敗しました\n");
	return true;
}

void VFXPlaySystem::WriteTimeParameter(MaterialHandle _material, float _elapsed, float _duration)
{
	VFXTimeParameter parameter{};
	parameter.progress = (std::min)(_elapsed / _duration, 1.0f);
	parameter.elapsed = _elapsed;
	parameter.duration = _duration;
	if (!GraphicsResourceManager::Instance().SetMaterialParameter(_material, VFX_TIME_PARAMETER_SLOT, &parameter, sizeof(parameter))) DEBUG_LOG_ERROR("VFXの時間の設定に失敗しました\n");
}
