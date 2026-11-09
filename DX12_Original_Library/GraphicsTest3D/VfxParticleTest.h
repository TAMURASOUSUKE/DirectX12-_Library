#pragma once
#include <vector>
#include <array>
#include <cstdint>
#include <cmath>
#include "../Src/Facade/TSLib.h"
#include "../Src/Facade/Compute.h"

// VFX04（GPU粒子）+ VFX05（再生管理）のテスト
// 同じシェーダー3本（ParticleUpdateCS / ParticleVS / ParticlePS）をパラメータだけ変えて、火花・煙・水の3種を出す
// 使う公開API（ライブラリ側）:
//   Gfx::SetMaterialParameter(material, 0, ParticleLookParameter)  b0 space1 に見た目のパラメータを置く（粒子Bufferはライブラリが枠ごとに結ぶ）
//   Gfx::RegisterVFXParticle(VFXParticleDesc)                      粒子の種類を登録し、同時再生数ぶんの枠（Buffer2本 + Materialの複製）を作る
//   Gfx::PlayVFX(particleType, position)                             空き枠を1つ借りて再生開始
//   Gfx::UpdateVFX(handle, dt)                                      経過を進め、Dispatchと描画登録を行う。終了した更新で false
//   Gfx::SetVFXPosition(handle, position)                           発生位置を動かす（発生速度は位置の差分からライブラリが出す）
//   Gfx::IsFinishedVFX(handle)                                      終了していれば true（無効ハンドルにも true）
struct VfxParticleTest
{
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

	static constexpr std::uint32_t PARTICLE_COUNT{ 4096 }; // 枠1つあたりの粒子数

	// 1種類ぶんの登録と、その再生の様子
	struct Kind
	{
		const char* name{ "" };
		MaterialBlendMode blendMode{ MaterialBlendMode::Additive };
		ParticleLookParameter look{};
		VFXParticleDesc desc{};           // material はテンプレート。ライブラリが枠ごとに複製して粒子Bufferを結ぶ
		float replayInterval{ 1.0f };     // この秒数ごとに新しく再生開始する（同時再生数の枠を使い回す様子を見る）
		Vector3 center{ Vector3::Zero };
		float orbitRadius{ 0.0f };        // 0なら動かない
		float orbitSpeed{ 0.0f };         // rad/s

		VFXParticleTypeHandle particleType{};
		std::vector<VFXHandle> playing{}; // 再生中のハンドル。終了したものは毎フレーム外す
		float replayTimer{ 0.0f };
		float orbitAngle{ 0.0f };
		std::uint32_t playCount{ 0 };     // 再生開始に成功した回数
		std::uint32_t rejectedCount{ 0 }; // 空き枠が無くて断られた回数
	};

	ShaderHandle resetCounterCS{};
	ShaderHandle updateCS{};
	ComputePipelineHandle resetCounterPipeline{};
	ComputePipelineHandle updatePipeline{};
	ShaderHandle particleVS{};
	ShaderHandle particlePS{};

	std::array<Kind, 3> kinds{};

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

		SetupSparks(kinds[0]);
		SetupSmoke(kinds[1]);
		SetupWater(kinds[2]);
		for (Kind& kind : kinds)
		{
			if (!RegisterKind(kind)) return false;
		}
		return true;
	}

	void Shutdown()
	{
		for (Kind& kind : kinds)
		{
			for (VFXHandle handle : kind.playing) Gfx::StopVFX(handle, true);
			kind.playing.clear();
			Gfx::Unload(kind.desc.material); // 枠ごとの複製はライブラリが持ち、ライブラリの終了で解放される
		}
		Gfx::Unload(particlePS);
		Gfx::Unload(particleVS);
		Compute::Unload(updatePipeline);
		Compute::Unload(resetCounterPipeline);
		Compute::Unload(updateCS);
		Compute::Unload(resetCounterCS);
	}

	// 毎フレーム：一定間隔で再生開始 → 再生中のものは位置を動かして更新 → 終了したものを外す
	// 描画登録は UpdateVFX の中でライブラリが行うので、Draw は無い
	void Update(float _deltaTime)
	{
		for (Kind& kind : kinds)
		{
			UpdateKind(kind, _deltaTime);
		}
	}

private:
	// 火花：加算、速度方向に伸びる尾、白→橙→赤、回る発生源から噴き上げる。2秒発生して消え、1.5秒ごとに再生開始（枠2つを交互に使う）
	static void SetupSparks(Kind& _kind)
	{
		_kind.name = "Sparks";
		_kind.blendMode = MaterialBlendMode::Additive;
		_kind.desc.duration = 2.0f;
		_kind.desc.emitPerFrame = 240;
		_kind.desc.maxInstances = 2;
		_kind.replayInterval = 1.5f;
		_kind.center = { 0.0f, 44.0f, 0.0f };
		_kind.orbitRadius = 3.0f;
		_kind.orbitSpeed = 2.5f;
		// desc.parameter と look は既定値が火花用
	}

	// 煙：アルファ合成、ゆっくり昇りながら膨らみ回る、渦の風に乗って流される（移流）。3秒発生、余韻4秒
	static void SetupSmoke(Kind& _kind)
	{
		_kind.name = "Smoke";
		_kind.blendMode = MaterialBlendMode::Blend;
		_kind.desc.duration = 3.0f;
		_kind.desc.emitPerFrame = 10;
		_kind.desc.maxInstances = 2;
		_kind.replayInterval = 4.0f;
		_kind.center = { -8.0f, 42.0f, 0.0f };

		ParticleUpdateParameter& p{ _kind.desc.parameter };
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

		ParticleLookParameter& l{ _kind.look };
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

	// 水：アルファ合成、斜め上へ噴き出して重力で落ち、床で跳ねる。白い芯で光る水滴。2.5秒発生、余韻1.8秒
	static void SetupWater(Kind& _kind)
	{
		_kind.name = "Water";
		_kind.blendMode = MaterialBlendMode::Blend;
		_kind.desc.duration = 2.5f;
		_kind.desc.emitPerFrame = 150;
		_kind.desc.maxInstances = 2;
		_kind.replayInterval = 3.0f;
		_kind.center = { 8.0f, 41.0f, 0.0f };

		ParticleUpdateParameter& p{ _kind.desc.parameter };
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

		ParticleLookParameter& l{ _kind.look };
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

	// テンプレートのMaterialを作って見た目を入れ、粒子の種類として登録する
	bool RegisterKind(Kind& _kind)
	{
		MaterialRenderDesc renderDesc{};
		renderDesc.blendMode = _kind.blendMode;
		renderDesc.doubleSided = true;
		_kind.desc.material = Gfx::CreateMaterial(particleVS, particlePS, renderDesc);
		if (!_kind.desc.material.IsValid()) return false;
		if (!Gfx::SetMaterialParameter(_kind.desc.material, 0, _kind.look)) return false;

		_kind.desc.updatePipeline = updatePipeline;
		_kind.desc.resetPipeline = resetCounterPipeline;
		_kind.desc.particleCount = PARTICLE_COUNT;

		_kind.particleType = Gfx::RegisterVFXParticle(_kind.desc);
		if (!_kind.particleType.IsValid())
		{
			DEBUG_LOG_ERROR("VFX粒子の種類の登録に失敗しました({})\n", _kind.name);
			return false;
		}
		_kind.replayTimer = _kind.replayInterval; // 最初のフレームで1つ目を再生開始する
		return true;
	}

	void UpdateKind(Kind& _kind, float _deltaTime)
	{
		// [数学] 角度を進めて円周上の点を取る。発生速度は位置の差分からライブラリが出すので、ここでは位置だけ渡す
		Vector3 position{ _kind.center };
		if (_kind.orbitRadius > 0.0f)
		{
			_kind.orbitAngle += _kind.orbitSpeed * _deltaTime;
			position = _kind.center + Vector3{ std::cos(_kind.orbitAngle) * _kind.orbitRadius, 0.0f, std::sin(_kind.orbitAngle) * _kind.orbitRadius };
		}

		// 1. 一定間隔で再生開始。空き枠が無ければ無効ハンドルが返る
		_kind.replayTimer += _deltaTime;
		if (_kind.replayTimer >= _kind.replayInterval)
		{
			_kind.replayTimer = 0.0f;
			const VFXHandle handle{ Gfx::PlayVFX(_kind.particleType, position) };
			if (handle.IsValid())
			{
				_kind.playing.push_back(handle);
				_kind.playCount++;
				DEBUG_LOG("VFX再生開始({}) 累計{}回 再生中{}個\n", _kind.name, _kind.playCount, _kind.playing.size());
			}
			else
			{
				_kind.rejectedCount++;
				DEBUG_LOG("VFX再生開始を断られました({}) 累計{}回\n", _kind.name, _kind.rejectedCount);
			}
		}

		// 2. 再生中のものは位置を渡して更新。終了（false）したものはハンドルを外す
		for (std::size_t i{ 0 }; i < _kind.playing.size();)
		{
			const VFXHandle handle{ _kind.playing[i] };
			Gfx::SetVFXPosition(handle, position);
			const bool alive{ Gfx::UpdateVFX(handle, _deltaTime) };
			if (!alive)
			{
				if (!Gfx::IsFinishedVFX(handle)) DEBUG_LOG_ERROR("UpdateVFXがfalseなのにIsFinishedVFXがfalseです({})\n", _kind.name);
				DEBUG_LOG("VFX終了({}) 再生中{}個\n", _kind.name, _kind.playing.size() - 1);
				_kind.playing[i] = _kind.playing.back();
				_kind.playing.pop_back();
			}
			else
			{
				++i;
			}
		}
	}
};
