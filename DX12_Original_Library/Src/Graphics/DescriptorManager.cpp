#include "DescriptorManager.h"

// シングルトン内部
DescriptorManager& DescriptorManager::Instance()
{
	static DescriptorManager instance;
	return instance;
}

void DescriptorManager::Initialize(ID3D12Device* _device)
{

}