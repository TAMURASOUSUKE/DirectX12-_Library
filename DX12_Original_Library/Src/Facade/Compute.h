#pragma once
#include <cstddef>
#include "../Core/Handle/ComputePipelineHandle.h"
#include "../Core/Handle/GPUBufferHandle.h"
#include "../Core/Handle/ShaderHandle.h"
#include "../Graphics/GfxType.h"

// GPUによる汎用計算機能の提供
namespace Compute
{
	// ComputeShaderを読み込む
	ShaderHandle LoadShader(const wchar_t* _filePath);

	// ComputeShaderからPipelineを作成する
	ComputePipelineHandle CreateComputePipeline(ShaderHandle _computeShader);

	// StructuredBufferを作成し、必要なら初期転送を予約する
	GPUBufferHandle CreateGPUBuffer(const GPUBufferDesc& _desc, const void* _initialData = nullptr, std::size_t _dataSize = 0);

	// ComputeShaderの解放
	void Unload(ShaderHandle _handle);

	// ComputePipelineの解放
	void Unload(ComputePipelineHandle _handle);

	// GPUBufferの解放
	void Unload(GPUBufferHandle _handle);
}
