#pragma once
#include <vector>
#include <array>
#include <cstdint>
#include <cstddef>
#include <cmath>
#include "../Src/Facade/TSLib.h"
#include "../Src/Facade/Compute.h"

// VFX04（GPU粒子）のテスト
// 同じシェーダー3本（ParticleUpdateCS / ParticleVS / ParticlePS）をパラメータだけ変えて、火花・煙・水の3種を出す
// 使う公開API（ライブラリ側）:
//   Gfx::SetMaterialBuffer(material, slot, GPUBufferHandle)        t0〜t3 space1 のスロットにBufferのSRVを置く
//   Gfx::SetMaterialParameter(material, 0, ParticleLookParameter)  b0 space1 に見た目のパラメータを置く
//   Gfx::DrawVFXInstanced(material, instanceCount, sortPosition)   DrawInstanced(4, instanceCount)
struct VfxParticleTest
{
	// ParticleUpdateCS.hlsl の cbuffer と同じ並び（float3+float は同じ16バイトに詰まるので、alignas(16)のVector3は使わずfloatで持つ）
	struct alignas(16) ParticleUpdateParameter
	{
		float emitPosition[3]{ 0.0f, 0.0f, 0.0f };
		float deltaTime{ 0.0f };
		float gravity[3]{ 0.0f, -9.8f, 0.0f };
		std::uint32_t emitCount{ 0 };
		std::uint32_t particleCount{ 0 };
		std::uint32_t randomSeed{ 0 };
		float lifeMin{ 0.5f };
		float lifeMax{ 1.5f };
		float speedMin{ 5.0f };
		float speedMax{ 12.0f };
		float size{ 0.1f };
		float drag{ 0.6f };
		float color[4]{ 1.0f, 0.6f, 0.2f, 1.0f };
		float emitVelocity[3]{ 0.0f, 0.0f, 0.0f };
		float swirl{ 0.0f };
		float coneDirection[3]{ 0.0f, 1.0f, 0.0f };
		float coneBias{ 0.8f };
		float spawnRadius{ 0.0f };
		float groundY{ -1.0e6f };
		float bounce{ 0.0f };
		float sizeSpread{ 0.4f };
	};
	static_assert(sizeof(ParticleUpdateParameter) == 128, "ParticleUpdateParameter must match the HLSL cbuffer (128 bytes)");

	// ParticleLook.hlsli の cbuffer と同じ並び（SetMaterialParameter slot0）
	struct alignas(16) ParticleLookParameter
	{
		float stretchPerSpeed{ 0.06f };
		float maxStretch{ 6.0f };
		float growth{ 0.0f };
		float rotationSpeed{ 0.0f };
		float glowStrength{ 0.8f };
		float coreStrength{ 1.6f };
		float tailFade{ 0.2f };
		float fadeInRatio{ 0.0f };
		float hotColor[3]{ 1.0f, 0.97f, 0.85f };
		float hotEnd{ 0.25f };
		float coldColor[3]{ 0.55f, 0.04f, 0.0f };
		float fadePower{ 2.0f };
		float additive{ 1.0f };
		float lookPadding[3]{ 0.0f, 0.0f, 0.0f };
	};
	static_assert(sizeof(ParticleLookParameter) == 80, "ParticleLookParameter must match the HLSL cbuffer (80 bytes)");

	static constexpr std::uint32_t PARTICLE_COUNT{ 16384 }; // Bufferの要素数 = 1種類あたり同時に存在できる最大数
	static constexpr std::uint32_t PARTICLE_STRIDE{ 64 };   // Particle.hlsli の Particle と同じ64バイト
	static constexpr std::uint32_t THREAD_PER_GROUP{ 256 }; // ParticleUpdateCS の numthreads

	// 1種類ぶんの発生源。Buffer・Material・パラメータを持つ（シェーダーとCompute PSOは全種で共有）
	struct Emitter
	{
		const char* name{ "" };
		ParticleUpdateParameter parameter{};
		ParticleLookParameter look{};
		MaterialBlendMode blendMode{ MaterialBlendMode::Additive };
		std::uint32_t emitPerFrame{ 0 };
		Vector3 emitCenter{ Vector3::Zero };
		float orbitRadius{ 0.0f }; // 0なら動かない
		float orbitSpeed{ 0.0f };  // rad/s
		float orbitAngle{ 0.0f };
		Vector3 emitPosition{ Vector3::Zero };

		GPUBufferHandle particleBuffer{};
		GPUBufferHandle emitCounterBuffer{};
		MaterialHandle material{};
	};

	ShaderHandle resetCounterCS{};
	ShaderHandle updateCS{};
	ComputePipelineHandle resetCounterPipeline{};
	ComputePipelineHandle updatePipeline{};
	ShaderHandle particleVS{};
	ShaderHandle particlePS{};

	std::array<Emitter, 3> emitters{};
	bool emitting{ true }; // キーで止められるように
	std::uint32_t frameSeed{ 1 };

	bool Setup()
	{
		// 共有：Compute側と描画側のシェーダー
		resetCounterCS = Compute::LoadShader(L"ParticleResetCounterCS.hlsl");
		updateCS = Compute::LoadShader(L"ParticleUpdateCS.hlsl");
		if (!resetCounterCS.IsValid() || !updateCS.IsValid()) return false;
		resetCounterPipeline = Compute::CreateComputePipeline(resetCounterCS);
		updatePipeline = Compute::CreateComputePipeline(updateCS);
		if (!resetCounterPipeline.IsValid() || !updatePipeline.IsValid()) return false;
		particleVS = Gfx::LoadShader(L"ParticleVS.hlsl", ShaderUsage::VFX, ShaderStage::Vertex);
		particlePS = Gfx::LoadShader(L"ParticlePS.hlsl", ShaderUsage::VFX, ShaderStage::Pixel);
		if (!particleVS.IsValid() || !particlePS.IsValid()) return false;

		SetupSparks(emitters[0]);
		SetupSmoke(emitters[1]);
		SetupWater(emitters[2]);
		for (Emitter& emitter : emitters)
		{
			if (!SetupEmitter(emitter)) return false;
		}
		return true;
	}

	void Shutdown()
	{
		for (Emitter& emitter : emitters)
		{
			Gfx::Unload(emitter.material);
			Compute::Unload(emitter.emitCounterBuffer);
			Compute::Unload(emitter.particleBuffer);
		}
		Gfx::Unload(particlePS);
		Gfx::Unload(particleVS);
		Compute::Unload(updatePipeline);
		Compute::Unload(resetCounterPipeline);
		Compute::Unload(updateCS);
		Compute::Unload(resetCounterCS);
	}

	// 毎フレーム：発生源を動かす → カウンタを0に戻す → 更新と発生（Dispatchは EndFrame で描画より前に実行される）
	void Update(float _deltaTime)
	{
		frameSeed++;
		for (Emitter& emitter : emitters)
		{
			UpdateEmitter(emitter, _deltaTime);
		}
	}

	// 毎フレーム：種類ごとに粒子数ぶんの板を1回のDrawで描く（死んでいる粒子はVSが大きさ0にする）
	void Draw()
	{
		for (Emitter& emitter : emitters)
		{
			Gfx::DrawVFXInstanced(emitter.material, PARTICLE_COUNT, emitter.emitPosition);
		}
	}

private:
	// 火花：加算、速度方向に伸びる尾、白→橙→赤、回る発生源から噴き上げる
	static void SetupSparks(Emitter& _emitter)
	{
		_emitter.name = "Sparks";
		_emitter.blendMode = MaterialBlendMode::Additive;
		_emitter.emitPerFrame = 240;
		_emitter.emitCenter = { 0.0f, 44.0f, 0.0f };
		_emitter.orbitRadius = 3.0f;
		_emitter.orbitSpeed = 2.5f;
		// parameter と look は既定値が火花用
	}

	// 煙：アルファ合成、ゆっくり昇りながら膨らみ回る、渦の風に乗って流される（移流）
	static void SetupSmoke(Emitter& _emitter)
	{
		_emitter.name = "Smoke";
		_emitter.blendMode = MaterialBlendMode::Blend;
		_emitter.emitPerFrame = 10;
		_emitter.emitCenter = { -8.0f, 42.0f, 0.0f };

		ParticleUpdateParameter& p{ _emitter.parameter };
		p.gravity[0] = 0.0f; p.gravity[1] = 1.2f; p.gravity[2] = 0.0f; // 浮力として上向き
		p.lifeMin = 2.5f;
		p.lifeMax = 4.0f;
		p.speedMin = 0.8f;
		p.speedMax = 1.8f;
		p.size = 0.9f;
		p.drag = 0.8f;
		p.color[0] = 0.55f; p.color[1] = 0.55f; p.color[2] = 0.6f; p.color[3] = 0.35f;
		p.swirl = 1.5f;
		p.coneDirection[0] = 0.0f; p.coneDirection[1] = 1.0f; p.coneDirection[2] = 0.0f;
		p.coneBias = 1.5f;
		p.spawnRadius = 0.4f;
		p.sizeSpread = 0.4f;

		ParticleLookParameter& l{ _emitter.look };
		l.stretchPerSpeed = 0.0f; // 伸ばさない
		l.growth = 2.0f;          // 寿命の終わりに3倍
		l.rotationSpeed = 0.6f;
		l.glowStrength = 1.0f;
		l.coreStrength = 0.0f;
		l.tailFade = 1.0f;        // 均一
		l.fadeInRatio = 0.15f;
		l.hotColor[0] = 0.8f; l.hotColor[1] = 0.8f; l.hotColor[2] = 0.85f;
		l.hotEnd = 0.2f;
		l.coldColor[0] = 0.3f; l.coldColor[1] = 0.3f; l.coldColor[2] = 0.35f;
		l.fadePower = 1.5f;
		l.additive = 0.0f;
	}

	// 水：アルファ合成、斜め上へ噴き出して重力で落ち、床で跳ねる。白い芯で光る水滴
	static void SetupWater(Emitter& _emitter)
	{
		_emitter.name = "Water";
		_emitter.blendMode = MaterialBlendMode::Blend;
		_emitter.emitPerFrame = 150;
		_emitter.emitCenter = { 8.0f, 41.0f, 0.0f };

		ParticleUpdateParameter& p{ _emitter.parameter };
		p.gravity[0] = 0.0f; p.gravity[1] = -20.0f; p.gravity[2] = 0.0f;
		p.lifeMin = 1.0f;
		p.lifeMax = 1.8f;
		p.speedMin = 6.0f;
		p.speedMax = 10.0f;
		p.size = 0.15f;
		p.drag = 0.1f;
		p.color[0] = 0.6f; p.color[1] = 0.8f; p.color[2] = 1.0f; p.color[3] = 0.8f;
		p.coneDirection[0] = 0.35f; p.coneDirection[1] = 1.0f; p.coneDirection[2] = 0.0f;
		p.coneBias = 2.5f; // 細い噴流
		p.spawnRadius = 0.1f;
		p.groundY = 40.0f;
		p.bounce = 0.35f;
		p.sizeSpread = 0.3f;

		ParticleLookParameter& l{ _emitter.look };
		l.stretchPerSpeed = 0.05f;
		l.maxStretch = 3.0f;
		l.glowStrength = 0.6f;
		l.coreStrength = 2.0f; // 白いハイライト
		l.tailFade = 0.5f;
		l.hotColor[0] = 1.0f; l.hotColor[1] = 1.0f; l.hotColor[2] = 1.0f;
		l.hotEnd = 0.1f;
		l.coldColor[0] = 0.3f; l.coldColor[1] = 0.5f; l.coldColor[2] = 0.9f;
		l.fadePower = 0.5f;
		l.additive = 0.0f;
	}

	bool SetupEmitter(Emitter& _emitter)
	{
		_emitter.emitPosition = _emitter.emitCenter;

		// 粒子Buffer：全要素0で作る（life = 0 なので最初は全部が空き）
		GPUBufferDesc particleDesc{};
		particleDesc.elementCount = PARTICLE_COUNT;
		particleDesc.elementStride = PARTICLE_STRIDE;
		particleDesc.allowUnorderedAccess = true;
		const std::vector<std::byte> zeroParticles(static_cast<std::size_t>(PARTICLE_COUNT) * PARTICLE_STRIDE);
		_emitter.particleBuffer = Compute::CreateGPUBuffer(particleDesc, zeroParticles.data(), zeroParticles.size());

		// 空きの取り合いカウンタ：uint 1個
		GPUBufferDesc counterDesc{};
		counterDesc.elementCount = 1;
		counterDesc.elementStride = sizeof(std::uint32_t);
		counterDesc.allowUnorderedAccess = true;
		const std::uint32_t zeroCounter{ 0 };
		_emitter.emitCounterBuffer = Compute::CreateGPUBuffer(counterDesc, &zeroCounter, sizeof(zeroCounter));
		if (!_emitter.particleBuffer.IsValid() || !_emitter.emitCounterBuffer.IsValid()) return false;

		// 描画側：共有のVS/PSからこの種類のMaterialを作り、粒子Bufferをslot0へ、見た目をslot0のパラメータへ
		MaterialRenderDesc renderDesc{};
		renderDesc.blendMode = _emitter.blendMode;
		renderDesc.doubleSided = true;
		_emitter.material = Gfx::CreateMaterial(particleVS, particlePS, renderDesc);
		if (!_emitter.material.IsValid()) return false;
		if (!Gfx::SetMaterialBuffer(_emitter.material, 0, _emitter.particleBuffer)) return false;
		if (!Gfx::SetMaterialParameter(_emitter.material, 0, _emitter.look)) return false;
		return true;
	}

	void UpdateEmitter(Emitter& _emitter, float _deltaTime)
	{
		// [数学] 角度を進めて円周上の点を取る。前回位置との差を時間で割ると発生源の速度になる
		Vector3 emitVelocity{ Vector3::Zero };
		if (_emitter.orbitRadius > 0.0f)
		{
			_emitter.orbitAngle += _emitter.orbitSpeed * _deltaTime;
			const Vector3 previous{ _emitter.emitPosition };
			_emitter.emitPosition = _emitter.emitCenter + Vector3{ std::cos(_emitter.orbitAngle) * _emitter.orbitRadius, 0.0f, std::sin(_emitter.orbitAngle) * _emitter.orbitRadius };
			if (_deltaTime > 0.0f) emitVelocity = (_emitter.emitPosition - previous) / _deltaTime;
		}

		// 1. カウンタを0へ（1スレッド）
		ComputeDispatchDesc resetDesc{};
		resetDesc.pipeline = resetCounterPipeline;
		resetDesc.outputs[0] = _emitter.emitCounterBuffer; // u0
		resetDesc.groupCountX = 1;
		if (!Compute::Dispatch(resetDesc)) DEBUG_LOG_ERROR("粒子カウンタのリセットDispatchに失敗しました({})\n", _emitter.name);

		// 2. 更新と発生（スレッド = 粒子）
		ParticleUpdateParameter parameter{ _emitter.parameter };
		parameter.emitPosition[0] = _emitter.emitPosition.x;
		parameter.emitPosition[1] = _emitter.emitPosition.y;
		parameter.emitPosition[2] = _emitter.emitPosition.z;
		parameter.deltaTime = _deltaTime;
		parameter.emitCount = emitting ? _emitter.emitPerFrame : 0;
		parameter.particleCount = PARTICLE_COUNT;
		parameter.randomSeed = frameSeed;
		parameter.emitVelocity[0] = emitVelocity.x;
		parameter.emitVelocity[1] = emitVelocity.y;
		parameter.emitVelocity[2] = emitVelocity.z;

		ComputeDispatchDesc updateDesc{};
		updateDesc.pipeline = updatePipeline;
		updateDesc.outputs[0] = _emitter.particleBuffer;    // u0
		updateDesc.outputs[1] = _emitter.emitCounterBuffer; // u1
		updateDesc.groupCountX = (PARTICLE_COUNT + THREAD_PER_GROUP - 1) / THREAD_PER_GROUP; // 切り上げ
		if (!Compute::Dispatch(updateDesc, &parameter, sizeof(parameter))) DEBUG_LOG_ERROR("粒子の更新Dispatchに失敗しました({})\n", _emitter.name);
	}
};
