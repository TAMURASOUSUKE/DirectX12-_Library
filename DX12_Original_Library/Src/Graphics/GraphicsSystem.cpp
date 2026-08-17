#include "../Debug/DebugLogs.h"
#include "GraphicsDevice.h"
#include "GraphicsResourceManager.h"
#include "CameraSystem.h"
#include "GraphicsSystem.h"

bool GraphicsSystem::Setup(GraphicsDevice* _device, GraphicsResourceManager* _resourceManager, CameraSystem* _cameraSystem, int _clientWidth, int _clientHeight, int _virtualWidth, int _virtualHeight)
{
	if (!_device || !_resourceManager || !_cameraSystem)
	{
		DEBUG_LOG_ERROR("GraphicsSystemに渡される参照値に不正な値があります\n");
		return false;
	}
	if (_clientWidth <= 0 || _clientHeight <= 0 || _virtualWidth <= 0 || _virtualHeight <= 0)
	{
		DEBUG_LOG_ERROR("画面サイズもしくは仮想解像度に不正な値があります\n");
		return false;
	}

	device = _device;
	resourceManager = _resourceManager;
	cameraSystem = _cameraSystem;

	screenSize = { _clientWidth, _clientHeight };
	virtualSize = { _virtualWidth, _virtualHeight };
	return true;
}

void GraphicsSystem::Shutdown()
{

}

void GraphicsSystem::RequestResize(int _width, int _height)
{

}

bool GraphicsSystem::ApplyPendingResize()
{

}
