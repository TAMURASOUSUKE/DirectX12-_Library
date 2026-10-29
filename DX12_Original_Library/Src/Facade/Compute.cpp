#include "Compute.h"

ShaderHandle Compute::LoadShader(const wchar_t* _filePath)
{
	return ShaderHandle{};
}

ComputePipelineHandle Compute::CreateComputePipeline(ShaderHandle _computeShader)
{
	return ComputePipelineHandle{};
}

GPUBufferHandle Compute::CreateGPUBuffer(const GPUBufferDesc& _desc, const void* _initialData, std::size_t _dataSize)
{
	return GPUBufferHandle{};
}

void Compute::Unload(ShaderHandle _handle)
{

}

void Compute::Unload(ComputePipelineHandle _handle)
{

}

void Compute::Unload(GPUBufferHandle _handle)
{

}
