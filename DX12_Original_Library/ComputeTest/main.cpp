#include <cstddef>
#include <vector>
#include "../Src/Facade/TSLib.h"

// Compute側に読ませる定数バッファ
struct alignas(16) ComputeParameter
{
	std::uint32_t elementCount{ 1024 }; // 要素数
	std::uint32_t addValue{ 4 }; // 加算値
	std::uint32_t padding[2]{ 0, 0 };
};


// エントリーポイント
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{

	// 初期化 失敗したら-1を返す
	if (!TSLib::Initialize(L"ComputeTest", 1280, 720)) return -1;
	// マウスカーソルの状態を設定
	bool isLocked{ false };
	if (!System::SetCursorMode(System::CursorMode::Normal)) DEBUG_LOG_ERROR("マウスカーソルの状態設定に失敗しました\n");

	// 今回一つのスレッドグループ内のスレッド数は64
	UINT threadNum{ 64 };
	// Compute用の設定構造体
	GPUBufferDesc desc{};
	desc.elementCount = 1024;
	desc.elementStride = 4;
	desc.allowUnorderedAccess = true;

	GPUBufferHandle gpuBufferA{ Compute::CreateGPUBuffer(desc) }; // 入力
	GPUBufferHandle gpuBufferB{ Compute::CreateGPUBuffer(desc) }; // 出力
	ShaderHandle fillCS{ Compute::LoadShader(L"FillCS.hlsl") };
	ShaderHandle scaleCS{ Compute::LoadShader(L"ScaleCS.hlsl") };
	ComputeParameter fillParam{ 1024, 7 };
	ComputeParameter scaleParam{ 1024, 0 };
	ComputePipelineHandle fillCSPipeline{ Compute::CreateComputePipeline(fillCS) };
	ComputePipelineHandle scaleCSPipeline{ Compute::CreateComputePipeline(scaleCS) };

	ComputeDispatchDesc fillDispatchDesc{};
	fillDispatchDesc.groupCountX = (desc.elementCount + threadNum - 1) / threadNum; // bufferなのでXだけ, 切り上げるためにスレッド数 - 1を足している
	fillDispatchDesc.outputs[0] = ComputeResourceBinding{ gpuBufferA };
	fillDispatchDesc.pipeline = fillCSPipeline;

	ComputeDispatchDesc scaleDispatchDesc{};
	scaleDispatchDesc.groupCountX = (desc.elementCount + threadNum - 1) / threadNum; // bufferなのでXだけ, 切り上げるためにスレッド数 - 1を足している
	scaleDispatchDesc.inputs[0] = ComputeResourceBinding{ gpuBufferA };
	scaleDispatchDesc.outputs[0] = ComputeResourceBinding{ gpuBufferB };
	scaleDispatchDesc.pipeline = scaleCSPipeline;

	std::vector<std::uint32_t> result(desc.elementCount); // 結果を受け取る

	int frameCount{ 0 };
	while (TSLib::ProcessMessage() && !Input::IsKeyPushed(KeyCode::Button::ESC))
	{
		TSLib::BeginFrame(); // フレーム開始処理
		frameCount++;

		// 前回フレームの結果を読み取る(最初のフレームでは存在しないので失敗します)
		if (!Compute::ReadBackBuffer(gpuBufferB, result.data(), result.size() * sizeof(std::uint32_t))) DEBUG_LOG_ERROR("Readbackに失敗しました\n");

		// 結果をチェック
		std::uint32_t missmatchCount{ 0 };
		for (std::uint32_t i = 0; i < desc.elementCount; i++)
		{
			const std::uint32_t expected{ (i * 2 + fillParam.addValue) * 3 };
			if (result[i] == expected) continue;
			if (missmatchCount == 0) DEBUG_LOG_ERROR("最初の不一致 index = {} actual = {} expected = {}\n", i, result[i], expected);
			missmatchCount++;
		}
		DEBUG_LOG("{}\n", missmatchCount == 0 ? "一致" : "不一致");

		if (frameCount == 50)
		{
			// 50フレームで両方消して作り直す
			Compute::Unload(gpuBufferA);
			Compute::Unload(gpuBufferB);
			gpuBufferA = Compute::CreateGPUBuffer(desc);
			gpuBufferB = Compute::CreateGPUBuffer(desc);
			fillDispatchDesc.outputs[0] = ComputeResourceBinding{ gpuBufferA };
			scaleDispatchDesc.inputs[0] = ComputeResourceBinding{ gpuBufferA };
			scaleDispatchDesc.outputs[0] = ComputeResourceBinding{ gpuBufferB };
		}

		 if (!Compute::Dispatch(fillDispatchDesc, &fillParam, sizeof(ComputeParameter))) DEBUG_LOG_ERROR("FillのDispatchに失敗しました\n");
		  if (!Compute::Dispatch(scaleDispatchDesc, &scaleParam, sizeof(ComputeParameter))) DEBUG_LOG_ERROR("ScaleのDispatchに失敗しました\n");
		TSLib::EndFrame(); // フレーム終了処理
	}

	TSLib::Finish(); // 終了
	return 0;
}
