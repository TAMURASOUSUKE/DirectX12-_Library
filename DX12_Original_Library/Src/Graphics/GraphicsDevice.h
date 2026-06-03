#pragma once
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include "GraphicsConstant.h"
using Microsoft::WRL::ComPtr;

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

// DX12の初期化や終了処理、フレームの最初の処理と最後の処理を担当する
class GraphicsDevice
{
public:
	// シングルトン化
	static GraphicsDevice& Instance();

	// デフォルトデストラクタ
	~GraphicsDevice() = default;

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
	// 現在のRTVハンドルを取得するGetter
	D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentRTV() const;
	// DSVのハンドルを取得するGetter
	D3D12_CPU_DESCRIPTOR_HANDLE GetDSV() const;


private:
	GraphicsDevice() = default; // 内部でのみのインスタンス

	// コピーの禁止
	GraphicsDevice(const GraphicsDevice&) = delete;
	GraphicsDevice& operator=(const GraphicsDevice&) = delete;

private:
	// GPUとの接続
	ComPtr<IDXGIFactory6> factory; // GPU列挙用
	ComPtr<ID3D12Device> device; // GPU通信の中心

	// 命令送信
	ComPtr<ID3D12CommandQueue> cmdQueue; // 命令の投入先
	ComPtr<ID3D12CommandAllocator> cmdAllocators[FRAME_BUFFER_COUNT]; // ダブルバッファ用のアロケーター
	ComPtr<ID3D12GraphicsCommandList> cmdList; // 命令記録
	ComPtr<ID3D12Fence> fence; // CPUとGPUの同期
	UINT64 fenceValues[FRAME_BUFFER_COUNT]{ 0, 0 }; // 各フレームの同期値
	UINT64 fenceValueCounter{ 0 }; // フェンス値をカウントする計測器

	// 画面表示
	ComPtr<IDXGISwapChain4> swapChain; // バッファ交換
	ComPtr<ID3D12Resource> backBuffers[FRAME_BUFFER_COUNT]; // ダブルバッファ
	ComPtr<ID3D12Resource> dsvResource; // 深度バッファ用のリソース
	ComPtr<ID3D12DescriptorHeap> rtvHeap; // RTV用のヒープ
	ComPtr<ID3D12DescriptorHeap> dsvHeap;
	UINT rtvDescriptorSize{ 0 }; // RTV一つのサイズ
	UINT currentFrameIndex{ 0 }; // 今どちらのバッファか

};