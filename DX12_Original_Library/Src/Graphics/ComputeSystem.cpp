#include "../Debug/DebugLogs.h"
#include "ComputeSystem.h"

bool ComputeSystem::Setup(ID3D12Device* _device)
{
	if (isInitialize)
	{
		DEBUG_LOG_ERROR("すでに初期化されています\n");
		return false;
	}

	if (!_device)
	{
		DEBUG_LOG_ERROR("渡されたDeviceが不正です\n");
		return false;
	}

	device = _device;
	isInitialize = true;
	return true;
}

void ComputeSystem::Shutdown()
{
	commands.clear();
	nullSRV = {};
	nullUAV = {};
	device = nullptr;
	isInitialize = false;
}

bool ComputeSystem::RegisterDispatch(const ComputeDispatchDesc& _desc, const void* _data, std::size_t _parameterSize)
{

}

bool ComputeSystem::Flush(ID3D12GraphicsCommandList* _cmd)
{

}

void ComputeSystem::Reset()
{
	commands.clear();
}
