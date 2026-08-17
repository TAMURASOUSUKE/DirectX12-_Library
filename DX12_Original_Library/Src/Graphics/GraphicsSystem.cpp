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

	// 二重SetupによるSceneRTのリークを防ぐ
	if (device || resourceManager || cameraSystem || sceneRenderTarget.IsValid())
	{
		DEBUG_LOG_ERROR("GraphicsSystemはすでにSetupされています\n");
		return false;
	}

	// ポインタ、サイズ設定済み、SceneRTHandle作成失敗という中途半端な状態を作らないように一時変数を使って対応
	const RTHandle temporarySceneRT{ _resourceManager->CreateRenderTarget(static_cast<UINT>(_clientWidth), static_cast<UINT>(_clientHeight)) };
	if (!temporarySceneRT.IsValid())
	{
		DEBUG_LOG_ERROR("GraphicsSystem用SceneRTの作成に失敗しました\n");
		return false;
	}
	// 有効ハンドルでも台帳失敗による異常を考える
	if (!_resourceManager->Lookup(temporarySceneRT))
	{
		DEBUG_LOG_ERROR("GraphicsSystem用SceneRTのLookupに失敗しました\n");
		_resourceManager->Unload(temporarySceneRT);
		return false;
	}

	// 全処理が成功しているので状態を確定する
	device = _device;
	resourceManager = _resourceManager;
	cameraSystem = _cameraSystem;

	sceneRenderTarget = temporarySceneRT;
	screenSize = { _clientWidth, _clientHeight };
	virtualSize = { _virtualWidth, _virtualHeight };

	pendingResizeSize = Vector2Int::Zero;
	hasPendingResize = false;
	return true;
}

void GraphicsSystem::Shutdown()
{
	// ResourceManagerがまだ生存している内に返却する
	if (resourceManager && sceneRenderTarget.IsValid()) resourceManager->Unload(sceneRenderTarget);

	sceneRenderTarget = {};
	screenSize = Vector2Int::Zero;
	virtualSize = Vector2Int::Zero;
	pendingResizeSize = Vector2Int::Zero;
	hasPendingResize = false;

	// 所有していない依存先との接続を切る
	cameraSystem = nullptr;
	resourceManager = nullptr;
	device = nullptr;
}

void GraphicsSystem::RequestResize(int _width, int _height)
{
	// 最小化すると0x0が届くためリサイズ要求として扱わない(最小化なのでエラーログは出さない)
	if (_width <= 0 || _height <= 0) return;
	
	// Setup前ならInitializeへ渡される初期サイズを使うため無視する
	if (!device || !resourceManager || !cameraSystem)
	{
		DEBUG_LOG_WARNING("Setup前にサイズ変更が要求されました\n");
		return;
	}

	// 新しい要求が来たら上書きする　ドラッグ中に何回来ても最後のサイズが残る
	pendingResizeSize = { _width, _height };
	hasPendingResize = true;
}

bool GraphicsSystem::ApplyPendingResize()
{
	// 予約がなければ正常終了
	if(!hasPendingResize) return true;

	// Resize中にメンバ状態を書き換えても影響しないようにコピーする
	const Vector2Int requestedSize{ pendingResizeSize };

	// 失敗時に毎フレーム自動再試行しないように先に要求を消費する
	pendingResizeSize = Vector2Int::Zero;
	hasPendingResize = false;

	if (!Resize(requestedSize.x, requestedSize.y))
	{
		DEBUG_LOG_ERROR("GraphicsSystemのリサイズに失敗しました Width : {} Height : {}\n", requestedSize.x, requestedSize.y);
		return false;
	}
	return true;
}

bool GraphicsSystem::Resize(int _width, int _height)
{
	if (!device || !resourceManager || !cameraSystem)
	{
		DEBUG_LOG_ERROR("GraphicsSystemがSetupされていません\n");
		return false;
	}
	if (_width <= 0 || _height <= 0)
	{
		DEBUG_LOG_ERROR("リサイズ先の画面サイズが不正です Width : {} Height : {}\n", _width, _height);
		return false;
	}

	// 同じサイズならGPUリソースを作る必要がないので正常終了
	if (screenSize.x == _width && screenSize.y == _height) return true;

	// 旧SceneRTを残したまま、新しいSceneRTを作成する
	const RTHandle temporarySceneRT{ resourceManager->CreateRenderTarget(static_cast<UINT>(_width), static_cast<UINT>(_height)) };
	if (!temporarySceneRT.IsValid())
	{
		DEBUG_LOG_ERROR("リサイズ用SceneRTの作成に失敗しました\n");
		return false;
	}
	// 有効ハンドルでも正しく分解して取得できるか確認
	if (!resourceManager->Lookup(temporarySceneRT))
	{
		DEBUG_LOG_ERROR("リサイズ用SceneRTのLookupに失敗しました\n");
		resourceManager->Unload(temporarySceneRT);
		return false;
	}

	const float prevAspectRatio{ static_cast<float>(screenSize.x) / static_cast<float>(screenSize.y) };  // 旧アスペクト比
	const float newAspectRatio{ static_cast<float>(_width) / static_cast<float>(_height) }; // 新しく作るサイズのアスペクト比

	// Deviceリサイズより先に検証可能なCameraを更新する
	if (!cameraSystem->SetAspectRatio(newAspectRatio)) // アスペクト比を新しくする
	{
		resourceManager->Unload(temporarySceneRT);
		return false;
	}

	// SwapChain・BackBuffer・DepthBufferを更新
	if (!device->Resize(static_cast<UINT>(_width), static_cast<UINT>(_height)))
	{
		DEBUG_LOG_ERROR("GraphicsDeviceのリサイズに失敗しました\n");
		// カメラの比率を戻す
		if (!cameraSystem->SetAspectRatio(prevAspectRatio)) DEBUG_LOG_ERROR("カメラのアスペクト比を元に戻せませんでした\n");
		resourceManager->Unload(temporarySceneRT);
		return false;
	}

	// ここまで全成功したため、新しい状態を確定する
	const RTHandle prevSceneRT{ sceneRenderTarget };
	sceneRenderTarget = temporarySceneRT;
	screenSize = { _width, _height };

	// 新しいSceneRTへ切り替えた後で旧SceneRTを解放待ちへ送る
	if (prevSceneRT.IsValid()) resourceManager->Unload(prevSceneRT);
	return true;
}
