#include "ShaderSystem.h"

void ShaderSystem::Initialize(ID3D12Device* _device)
{
	// 作成されたデバイスと結合
	if (_device != nullptr)
	{
		device = _device;
	}
}