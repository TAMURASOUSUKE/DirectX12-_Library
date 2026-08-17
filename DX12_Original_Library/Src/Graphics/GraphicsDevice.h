#pragma once
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <functional>
#include "GraphicsConstant.h"
using Microsoft::WRL::ComPtr;

// DX12の初期化や終了処理、フレームの最初の処理と最後の処理を担当する
class GraphicsDevice
{
public:
	// シングルトン化
	static GraphicsDevice& Instance();

	// デフォルトデストラクタ
	~GraphicsDevice() = default;

	// 初期化処理(ウィンドウハンドルと画面横サイズ、縦サイズ)
	void Setup(HWND _hwnd, int _width, int _height);
	// GPUが待機済みを前提に、内部オブジェクトを解放する終了処理
	void Shutdown();
	// GPU待機処理
	bool WaitForGPU();

	// フレームの最初に呼び出す関数
	void BeginFrame();
	// フレームの最後に呼び出す関数
	// フレームの命令送信、表示、Fence通知を行う全て成功した場合はtrue
	bool EndFrame();

	// 垂直同期の有効状態を設定する
	void SetVSync(bool _isEnabled) { isVSyncEnabled = _isEnabled; }

	// GPUの描画先を指定サイズで作り直す
	bool Resize(UINT _width, UINT _height);

	// デバイスのGetter
	ID3D12Device* GetDevice() const;
	// コマンドリストのGetter
	ID3D12GraphicsCommandList* GetCommandList() const;
	// 現在のRTVハンドルを取得するGetter
	D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentRTV() const;
	// DSVのハンドルを取得するGetter
	D3D12_CPU_DESCRIPTOR_HANDLE GetDSV() const;
	// 現在のフレームインデックスを取得する
	UINT GetCurrentFrameIndex() const;

	// GPUが処理完了したところまでのフェンス値を取得する
	UINT64 GetCompletedFenceValue() const;

	// CPU側が最後にGPUへ通知したフェンス値を取得する
	UINT64 GetLastSubmittedFenceValue() const;

	// ヘルパー
	HRESULT ExecuteUpdate(std::function<void(ID3D12GraphicsCommandList*)> _recode); // アップロードヘルパー

private:
	GraphicsDevice() = default; // 内部でのみのインスタンス

	// コピーの禁止
	GraphicsDevice(const GraphicsDevice&) = delete;
	GraphicsDevice& operator=(const GraphicsDevice&) = delete;

	// SwapChainからBackBufferを取得してRTVを作成する
	bool CreateBackBufferView();
	//　指定サイズの深度バッファを作る
	bool CreateDepthStencilBuffer(UINT _width, UINT _height);

private:
	bool isVSyncEnabled{ true }; // デフォルトは垂直同期を有効にする

	// GPUとの接続
	ComPtr<IDXGIFactory6> factory; // GPU列挙用
	ComPtr<ID3D12Device> device; // GPU通信の中心

	// 命令送信
	ComPtr<ID3D12CommandQueue> cmdQueue; // 命令の投入先
	ComPtr<ID3D12CommandAllocator> cmdAllocators[FRAME_BUFFER_COUNT]; // ダブルバッファ用のアロケーター
	ComPtr<ID3D12GraphicsCommandList> cmdList; // 命令記録
	ComPtr<ID3D12Fence> fence; // CPUとGPUの同期
	ComPtr<ID3D12CommandAllocator> uploadCmdAllocator{}; // 描画中でもリソースをUploadできるよう描画用と分離
	ComPtr<ID3D12GraphicsCommandList> uploadCmdList{}; // 描画中でもリソースをUploadできるよう描画用と分離
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
