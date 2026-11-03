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

// ComputeでtextureのUAVを検証するためのバッファ
struct alignas(16) TextureParameter
{
	std::uint32_t width{ 1920 }; // 横幅
	std::uint32_t height{ 1080 }; // 縦幅
	std::uint32_t padding[2]{0, 0};
};

// エントリーポイント
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{

	// 初期化 失敗したら-1を返す
	if (!TSLib::Initialize(L"ComputeTest", 1280, 720)) return -1;
	// マウスカーソルの状態を設定
	bool isLocked{ false };
	if (!System::SetCursorMode(System::CursorMode::Normal)) DEBUG_LOG_ERROR("マウスカーソルの状態設定に失敗しました\n");

	// BufferTest

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

	// TextureTest
	TextureParameter writeParam{ 100, 60 };
	TextureParameter readParam{ 100, 60 };

	TextureDesc texDesc{}; // 入力用
	texDesc.width = writeParam.width;
	texDesc.height = writeParam.height;
	texDesc.format = TextureFormat::R32_FLOAT; // Bufferに出力するため

	GPUBufferDesc gpuDesc{}; // 出力用
	gpuDesc.elementCount = texDesc.width * texDesc.height;
	gpuDesc.elementStride = 4;
	gpuDesc.allowUnorderedAccess = true; // UAVとして書き込むので許可

	ShaderHandle writeCS{ Compute::LoadShader(L"WriteTextureCS.hlsl") };
	ShaderHandle readCS{ Compute::LoadShader(L"ReadCS.hlsl") };
	TexHandle textureA{ Compute::CreateTexture(texDesc) }; // 入力
	GPUBufferHandle bufferB{ Compute::CreateGPUBuffer(gpuDesc) }; // 出力
	ComputePipelineHandle writePSO{ Compute::CreateComputePipeline(writeCS) };
	ComputePipelineHandle readPSO{ Compute::CreateComputePipeline(readCS) };

	ComputeDispatchDesc writeDesc{};
	writeDesc.groupCountX = (texDesc.width + 7) / 8;
	writeDesc.groupCountY = (texDesc.height + 7) / 8;
	writeDesc.outputs[0] = ComputeResourceBinding{ textureA }; // Textureを出力
	writeDesc.pipeline = writePSO;

	ComputeDispatchDesc readDesc{};
	readDesc.groupCountX = (texDesc.width + 7) / 8;
	readDesc.groupCountY = (texDesc.height + 7) / 8;
	readDesc.inputs[0] = ComputeResourceBinding{ textureA };
	readDesc.outputs[0] = ComputeResourceBinding{ bufferB };
	readDesc.pipeline = readPSO;

	std::vector<float> textureResult(gpuDesc.elementCount); // 結果を受け取る

	int frameCount{ 0 };
	while (TSLib::ProcessMessage() && !Input::IsKeyPushed(KeyCode::Button::ESC))
	{
		TSLib::BeginFrame(); // フレーム開始処理
		frameCount++;

		// 前回フレームの結果を読み取る(最初のフレームでは存在しないので失敗します)
		if (!Compute::ReadBackBuffer(gpuBufferB, result.data(), result.size() * sizeof(std::uint32_t))) DEBUG_LOG_ERROR("Readbackに失敗しました\n");
		if (!Compute::ReadBackBuffer(bufferB, textureResult.data(), textureResult.size() * sizeof(float))) DEBUG_LOG_ERROR("Texture出力Readbackに失敗しました\n");

		// Buffer結果をチェック
		std::uint32_t missmatchCount{ 0 };
		for (std::uint32_t i = 0; i < desc.elementCount; i++)
		{
			const std::uint32_t expected{ (i * 2 + fillParam.addValue) * 3 };
			if (result[i] == expected) continue;
			if (missmatchCount == 0 && frameCount != 1) DEBUG_LOG_ERROR("最初の不一致 index = {} actual = {} expected = {}\n", i, result[i], expected);
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

		// Texture結果をチェック
		std::uint32_t missmatchTextureCount{ 0 };
		for (std::uint32_t y = 0; y < texDesc.height; y++)
		{
			for (std::uint32_t x = 0; x < texDesc.width; x++)
			{
				const float expected{ static_cast<float>(x + y * texDesc.width) };
				if (textureResult[x + y * texDesc.width] == expected) continue;
				if (missmatchTextureCount == 0 && frameCount != 1) DEBUG_LOG_ERROR("最初の不一致 index = {} actual = {} expected = {}\n", x + y * texDesc.width, textureResult[x + y * texDesc.width], expected);
				missmatchTextureCount++;
			}
		}
		DEBUG_LOG("{}\n", missmatchTextureCount == 0 ? "一致" : "不一致");

		 if (!Compute::Dispatch(fillDispatchDesc, &fillParam, sizeof(ComputeParameter))) DEBUG_LOG_ERROR("FillのDispatchに失敗しました\n");
		 if (!Compute::Dispatch(scaleDispatchDesc, &scaleParam, sizeof(ComputeParameter))) DEBUG_LOG_ERROR("ScaleのDispatchに失敗しました\n");
		 if (!Compute::Dispatch(writeDesc, &writeParam, sizeof(TextureParameter))) DEBUG_LOG_ERROR("Texture書き込みのDispatchに失敗しました\n");
		 if (!Compute::Dispatch(readDesc, &readParam, sizeof(TextureParameter))) DEBUG_LOG_ERROR("texture出力読み込みのDispatchに失敗しました\n");
		TSLib::EndFrame(); // フレーム終了処理
	}

	TSLib::Finish(); // 終了
	return 0;
}
