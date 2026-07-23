#include <cstring>
#include "GraphicsDevice.h"
#include "GPUMarker.h"

// constructor
GPUMarker::GPUMarker(const char* _name) : cmdList{ GraphicsDevice::Instance().GetCommandList() }
{
	// Metadata = 1 -> Ansi文字列　Size = Nul終端込みのバイト数
	cmdList->BeginEvent(1, _name, static_cast<UINT>(std::strlen(_name) + 1));
}

GPUMarker::~GPUMarker()
{
	cmdList->EndEvent();
}