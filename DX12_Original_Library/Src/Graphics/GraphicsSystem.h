#pragma once
#include "../Core/Handle/RTHandle.h"
#include "../Math/Vector/Vector2Int.h"

// ポインタとして保持するだけなので前方宣言
class GraphicsDevice;
class GraphicsResourceManager;
class CameraSystem;

// Graphics全体の状態と各描画モジュールの実行順を統括する
class GraphicsSystem
{
public:
	GraphicsSystem() = default;
	~GraphicsSystem() = default;

	GraphicsSystem(const GraphicsSystem&) = delete;
	GraphicsSystem& operator=(const GraphicsSystem&) = delete;

	// Graphics全体で共有する依存先と画面サイズを設定
	bool Setup(GraphicsDevice* _device, GraphicsResourceManager* _resourceManager, CameraSystem* _cameraSystem, int _clientWidth, int _clientHeight, int _virtualWidth, int _virtualHeight);

	// 所有しているGraphics用リソースを解放する
	void Shutdown();

	// 次フレーム開始前に適用するサイズを予約する
	void RequestResize(int _width, int _height);

	// 予約されているリサイズを安全なタイミングで実行する
	bool ApplyPendingResize();

	// 現在の実描画サイズ
	Vector2Int GetScreenSize() const { return screenSize; }
	// 2D描画で使用する仮想解像度
	Vector2Int GetVirtualSize() const { return virtualSize; }
	// Scene全体を描く内部RenderTarget
	RTHandle GetSceneRenderTarget() const { return sceneRenderTarget; }

private:
	// Backbuffer・Depth・SceneRT・Cameraをまとめて変更する
	bool Resize(int _width, int _height);

private:
	// 所有しない依存先
	GraphicsDevice* device{ nullptr }; // フレーム処理やDX12の初期化など
	GraphicsResourceManager* resourceManager{ nullptr }; // グラフィックに関するリソース管理の集約
	CameraSystem* cameraSystem{ nullptr }; // カメラを制御するシステム

	RTHandle sceneRenderTarget{}; // GraphicsSystemが所有するサイズ依存リソース

	Vector2Int screenSize{ Vector2Int::Zero }; // 実際のスクリーンサイズ
	Vector2Int virtualSize{ Vector2Int::Zero }; // 仮想サイズ

	Vector2Int pendingResizeSize{ Vector2Int::Zero }; // リサイズする値を保持しておく
	bool hasPendingResize{ false }; // リサイズする値を持っているか

};


