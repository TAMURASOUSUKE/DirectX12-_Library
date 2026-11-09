#include <wrl/client.h>
#include <utility>
#include "../Graphics/GraphicsDevice.h"
#include "../Graphics/GraphicsResourceManager.h"
#include "../Graphics/ComputeSystem.h"
#include "../Graphics/PipelineFactory.h"
#include "../Debug/DebugLogs.h"
#include "ComputeInternal.h"
#include "Compute.h"
using Microsoft::WRL::ComPtr;

namespace
{
	ComputeSystem computeSystem{};
	ComPtr<ID3D12RootSignature> rootSig{}; // PSO設定の際に行う共通したルートシグネチャ
}

bool ComputeInternal::Initialize()
{
	const GraphicsDevice& graphicsDevice{ GraphicsDevice::Instance() };
	if (!graphicsDevice.GetDevice())
	{
		DEBUG_LOG_ERROR("ComputeSystemに不正なDeviceが渡されました\n");
		return false;
	}

	rootSig = PipelineFactory::BuildComputeRootSignature(graphicsDevice.GetDevice());
	if (!rootSig)
	{
		DEBUG_LOG_ERROR("Computeで使われる共通するRootSignatureの作成に失敗しました\n");
		return false;
	}

	if (!computeSystem.Setup(graphicsDevice.GetDevice()))
	{
		DEBUG_LOG_ERROR("ComputeSystemの初期化に失敗しました\n");
		rootSig.Reset();
		return false;
	}

	return true;
}

void ComputeInternal::BeginFrame()
{
	computeSystem.Reset();
}

void ComputeInternal::EndFrame()
{
	// 実行要求がある場合のみ行う
	if (!computeSystem.HasPendingDispatch()) return;
	const bool isFlush{ computeSystem.Flush(GraphicsDevice::Instance().GetCommandList()) };
	DEBUG_ASSERT(isFlush && "ComputeのFlushに失敗しました\n"); // Debug時にはAssertで止めRelease時には矛盾のない状態になる(GPUやリソースを壊すことがないためこの形)
}

void ComputeInternal::Finish()
{
	computeSystem.Shutdown();
	rootSig.Reset();
}

void ComputeInternal::RegisterDispatches(const std::vector<ComputeDispatchRequest>& _requests)
{
	for (const ComputeDispatchRequest& request : _requests)
	{
		const void* data{ request.parameterSize > 0 ? request.parameter.data() : nullptr };
		if (!computeSystem.RegisterDispatch(request.desc, data, request.parameterSize)) DEBUG_LOG_ERROR("仲介されたComputeの実行要求の登録に失敗しました\n");
	}
}

ShaderHandle Compute::LoadShader(const wchar_t* _filePath)
{
	if (!_filePath || _filePath[0] == L'\0')
	{
		DEBUG_LOG_ERROR("FilePathが不正です\n");
		return ShaderHandle{};
	}

	ComPtr<ID3DBlob> blob{ PipelineFactory::Compile(_filePath, "main", "cs_5_1") };
	if (!blob)
	{
		DEBUG_LOG_ERROR("Shaderのコンパイルに失敗しました\n");
		return ShaderHandle{};
	}
	ShaderHandle handle{ GraphicsResourceManager::Instance().RegisterShader(ShaderUsage::Compute, ShaderStage::Compute, std::move(blob)) };
	if (!handle.IsValid())
	{
		DEBUG_LOG_ERROR("作成されたハンドルが不正です\n");
		return ShaderHandle{};
	}
	return handle;
}

ComputePipelineHandle Compute::CreateComputePipeline(ShaderHandle _computeShader)
{
	if (!_computeShader.IsValid())
	{
		DEBUG_LOG_ERROR("渡されたCSのShaderHandleが不正です\n");
		return ComputePipelineHandle{};
	}

	ShaderData* shaderData{ GraphicsResourceManager::Instance().Lookup(_computeShader) };
	if (!shaderData)
	{
		DEBUG_LOG_ERROR("渡されたHandleのLookupに失敗しました\n");
		return ComputePipelineHandle{};
	}
	
	if (shaderData->usage != ShaderUsage::Compute || shaderData->stage != ShaderStage::Compute)
	{
		DEBUG_LOG_ERROR("Shaderの用途若しくはStageが不正です\n");
		return ComputePipelineHandle{};
	}

	ComPtr<ID3D12PipelineState> pipelineState{ PipelineFactory::BuildComputePipeline(GraphicsDevice::Instance().GetDevice() , shaderData->blob.Get(), rootSig.Get())};
	if (!pipelineState)
	{
		DEBUG_LOG_ERROR("作成されたPSOが不正です\n");
		return ComputePipelineHandle{};
	}
	ComputePipelineData pipelineData{};
	pipelineData.pipelineState = pipelineState;
	pipelineData.rootSignature = rootSig;
	ComputePipelineHandle handle{ GraphicsResourceManager::Instance().RegisterComputePipeline(pipelineData) };
	if (!handle.IsValid())
	{
		DEBUG_LOG_ERROR("生成されたComputePipelineHandleが不正です\n");
		return ComputePipelineHandle{};
	}
	return handle;
}

GPUBufferHandle Compute::CreateGPUBuffer(const GPUBufferDesc& _desc, const void* _initialData, std::size_t _dataSize)
{
	return GraphicsResourceManager::Instance().CreateGPUBuffer(_desc, _initialData, _dataSize);
}

TexHandle Compute::CreateTexture(const TextureDesc& _desc)
{
	return GraphicsResourceManager::Instance().CreateUAVTexture(_desc);
}

bool Compute::Dispatch(const ComputeDispatchDesc& _desc, const void* _data, std::size_t _parameterSize)
{
	if (!computeSystem.RegisterDispatch(_desc, _data, _parameterSize))
	{
		DEBUG_LOG_ERROR("Computeの実行予約に失敗しました\n");
		return false;
	}
	return true;
}

bool Compute::ReadBackBuffer(GPUBufferHandle _handle, void* _dst, std::size_t _size)
{
	return GraphicsResourceManager::Instance().ReadBackBuffer(_handle, _dst, _size);
}

void Compute::Unload(ShaderHandle _handle)
{
	GraphicsResourceManager::Instance().Unload(_handle);
}

void Compute::Unload(ComputePipelineHandle _handle)
{
	GraphicsResourceManager::Instance().Unload(_handle);
}

void Compute::Unload(GPUBufferHandle _handle)
{
	GraphicsResourceManager::Instance().Unload(_handle);
}
