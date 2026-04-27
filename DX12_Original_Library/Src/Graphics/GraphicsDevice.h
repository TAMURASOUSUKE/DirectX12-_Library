#pragma once
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

// DX12の初期化や終了処理、フレームの最初の処理と最後の処理を担当する
class GraphicsDevice
{
public:
	// シングルトン化
	static GraphicsDevice& Instance();

	// 初期化処理(ウィンドウハンドルと画面横サイズ、縦サイズ)
	void Initialize(HWND _hwnd, int _width, int _height);
	// 終了処理
	void Shutdown();

	// フレームの最初に呼び出す関数
	void BeginFrame();
	// フレームの最後に呼び出す関数
	void EndFrame();

	// デバイスのGetter
	ID3D12Device* GetDevice() const;
	// コマンドリストのGetter
	ID3D12GraphicsCommandList* GetCommandList() const;
};