#pragma once
#include <cstdint>
#include <cstdio>
#include <vector>
#include <windows.h>
#include "../Src/Facade/TSLib.h"
#include "../Src/Facade/GfxInternal.h"
#include "../Src/Graphics/GraphicsMetrics.h"

// VFX07 同時再生とMaterial Shaderの作り直しを繰り返し 計測値を集める
// 決めた秒数が過ぎたら平均と最大を1回だけ出力する(ReleaseでもOutputDebugStringで出る)
struct VfxStressTest
{
	// VfxPlayMeshTestPS.hlsl の cbuffer と同じ並び
	struct alignas(16) LookParameter
	{
		float baseColor[4]{ 0.4f, 0.8f, 1.0f, 0.8f }; // 色
		float flowSpeed{ 0.5f }; // 模様が横へ流れる速さ
		float edgeWidth{ 0.05f }; // 溶ける境目のぼかし幅
		float dissolve{ 1.0f }; // 1でprogressに合わせて溶ける
		float lookPadding{ 0.0f }; // 16バイトにそろえる詰め物
	};
	static_assert(sizeof(LookParameter) == 32, "LookParameter must match the HLSL cbuffer (32 bytes)");

	static constexpr std::uint32_t PLAY_COUNT{ 10 }; // 同時に再生し続ける数
	static constexpr float RUN_SECONDS{ 60.0f }; // 繰り返す秒数
	static constexpr float RECREATE_INTERVAL{ 1.0f }; // MaterialとShaderを作り直す間隔の秒数

	ShaderHandle pixelShader{}; // 再生に使うShader
	MaterialHandle material{}; // 再生に使うMaterial(テンプレート)
	VFXMeshTypeHandle meshType{}; // 再生する板の種類
	std::vector<VFXHandle> playing{}; // 再生中のハンドル
	float elapsed{ 0.0f }; // 開始からの秒数
	float recreateTimer{ 0.0f }; // 次の作り直しまでの秒数
	bool isFinished{ false }; // 結果を出力し終えたか

	std::uint32_t playCount{ 0 }; // 再生開始に成功した回数
	std::uint32_t rejectedCount{ 0 }; // 空きSlotが無くて断られた回数
	std::uint32_t recreateCount{ 0 }; // 作り直しに成功した回数
	std::uint32_t recreateFailedCount{ 0 }; // 作り直しに失敗した回数
	std::uint32_t frameCount{ 0 }; // 数えたフレーム数
	std::uint32_t gpuSampleCount{ 0 }; // GPU時間が有効だったフレーム数
	double vfxGpuSum{ 0.0 }; // VFXMeshパスのGPU時間の合計
	double vfxGpuMax{ 0.0 }; // VFXMeshパスのGPU時間の最大
	double computeGpuSum{ 0.0 }; // ComputeパスのGPU時間の合計
	double computeGpuMax{ 0.0 }; // ComputeパスのGPU時間の最大
	double vfxCpuSum{ 0.0 }; // VFXメッシュ描画の記録にかかったCPU時間の合計
	double vfxCpuMax{ 0.0 }; // VFXメッシュ描画の記録にかかったCPU時間の最大
	std::uint64_t vfxDrawSum{ 0 }; // VFXMeshパスのドローコールの合計
	std::uint64_t dispatchSum{ 0 }; // Dispatchの合計
	std::uint64_t drawSum{ 0 }; // 全体のドローコールの合計

	bool Setup()
	{
		pixelShader = Gfx::LoadShader(L"VfxPlayMeshTestPS.hlsl", ShaderUsage::VFX, ShaderStage::Pixel);
		if (!pixelShader.IsValid()) return false;

		MaterialRenderDesc renderDesc{};
		renderDesc.blendMode = MaterialBlendMode::Additive;
		renderDesc.doubleSided = true;
		material = Gfx::CreateMaterial(pixelShader, renderDesc);
		if (!material.IsValid()) return false;
		if (!Gfx::SetMaterialParameter(material, 0, LookParameter{})) return false;

		VFXMeshDesc desc{};
		desc.material = material;
		desc.kind = VFXMeshKind::Quad;
		desc.billboardMode = BillboardMode::FaceCamera;
		desc.width = 1.0f;
		desc.height = 1.0f;
		desc.duration = 0.5f;
		desc.maxInstances = PLAY_COUNT;
		meshType = Gfx::RegisterVFXMesh(desc);
		if (!meshType.IsValid()) return false;

		playing.reserve(PLAY_COUNT);
		return true;
	}

	void Shutdown()
	{
		for (VFXHandle handle : playing) Gfx::StopVFX(handle, true);
		playing.clear();
		Gfx::Unload(material);
		Gfx::Unload(pixelShader);
	}

	void Update(float _deltaTime)
	{
		if (isFinished) return;
		elapsed += _deltaTime;

		// 終了したものを外してから 常にPLAY_COUNT個が再生中になるように足す
		for (std::size_t i{ 0 }; i < playing.size();)
		{
			if (!Gfx::UpdateVFX(playing[i], _deltaTime))
			{
				playing[i] = playing.back();
				playing.pop_back();
			}
			else
			{
				++i;
			}
		}
		while (playing.size() < PLAY_COUNT)
		{
			const float column{ static_cast<float>(playCount % PLAY_COUNT) };
			const VFXHandle handle{ Gfx::PlayVFX(meshType, Vector3{ column * 1.2f - 5.4f, 60.0f, 0.0f }, Quaternion::Identity) };
			if (!handle.IsValid())
			{
				rejectedCount++;
				break;
			}
			playing.push_back(handle);
			playCount++;
		}

		// 一定の間隔で ShaderとMaterialを新しく作って捨てる
		recreateTimer += _deltaTime;
		if (recreateTimer >= RECREATE_INTERVAL)
		{
			recreateTimer = 0.0f;
			Recreate();
		}

		Sample();

		if (elapsed >= RUN_SECONDS)
		{
			Report();
			isFinished = true;
		}
	}

private:
	// ShaderとMaterialを作り 値を入れて すぐ解放する
	void Recreate()
	{
		const ShaderHandle shader{ Gfx::LoadShader(L"VfxPlayMeshTestPS.hlsl", ShaderUsage::VFX, ShaderStage::Pixel) };
		MaterialRenderDesc renderDesc{};
		renderDesc.blendMode = MaterialBlendMode::Blend;
		const MaterialHandle created{ shader.IsValid() ? Gfx::CreateMaterial(shader, renderDesc) : MaterialHandle{} };
		const bool isCreated{ created.IsValid() && Gfx::SetMaterialParameter(created, 0, LookParameter{}) };
		if (created.IsValid()) Gfx::Unload(created);
		if (shader.IsValid()) Gfx::Unload(shader);
		if (isCreated) recreateCount++;
		else recreateFailedCount++;
	}

	// 前フレームの計測値を足し込む
	void Sample()
	{
		const std::size_t vfxIndex{ static_cast<std::size_t>(GraphicsPass::VFXMesh) };
		const std::size_t computeIndex{ static_cast<std::size_t>(GraphicsPass::Compute) };
		const GraphicsFrameMetrics& metrics{ GfxInternal::GetLastFrameMetrics() };
		const GraphicsGPUTimingFrame& gpu{ GfxInternal::GetLastGPUTimingFrame() };

		frameCount++;
		vfxDrawSum += metrics.passDrawCallCount[vfxIndex];
		dispatchSum += metrics.passDrawCallCount[computeIndex];
		drawSum += metrics.drawCallCount;

		const double vfxCpu{ GfxInternal::GetLastVFXFlushCpuMilliseconds() };
		vfxCpuSum += vfxCpu;
		if (vfxCpu > vfxCpuMax) vfxCpuMax = vfxCpu;

		if (gpu.passes[vfxIndex].valid && gpu.passes[computeIndex].valid)
		{
			gpuSampleCount++;
			vfxGpuSum += gpu.passes[vfxIndex].milliseconds;
			computeGpuSum += gpu.passes[computeIndex].milliseconds;
			if (gpu.passes[vfxIndex].milliseconds > vfxGpuMax) vfxGpuMax = gpu.passes[vfxIndex].milliseconds;
			if (gpu.passes[computeIndex].milliseconds > computeGpuMax) computeGpuMax = gpu.passes[computeIndex].milliseconds;
		}
	}

	// 平均と最大を1回だけ出力する
	void Report()
	{
		const double frames{ frameCount > 0 ? static_cast<double>(frameCount) : 1.0 };
		const double gpuFrames{ gpuSampleCount > 0 ? static_cast<double>(gpuSampleCount) : 1.0 };
		char text[768]{};
		std::snprintf(text, sizeof(text),
			"[VFX07] seconds=%.1f frames=%u play=%u rejected=%u recreate=%u recreateFailed=%u "
			"vfxGpuAvgMs=%.4f vfxGpuMaxMs=%.4f computeGpuAvgMs=%.4f computeGpuMaxMs=%.4f "
			"vfxCpuAvgMs=%.4f vfxCpuMaxMs=%.4f vfxDrawAvg=%.2f dispatchAvg=%.2f drawAvg=%.2f gpuSamples=%u\n",
			elapsed, frameCount, playCount, rejectedCount, recreateCount, recreateFailedCount,
			vfxGpuSum / gpuFrames, vfxGpuMax, computeGpuSum / gpuFrames, computeGpuMax,
			vfxCpuSum / frames, vfxCpuMax, static_cast<double>(vfxDrawSum) / frames, static_cast<double>(dispatchSum) / frames, static_cast<double>(drawSum) / frames, gpuSampleCount);
		OutputDebugStringA(text);
	}
};
