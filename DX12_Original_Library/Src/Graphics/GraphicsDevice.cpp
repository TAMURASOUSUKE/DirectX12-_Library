#include "GraphicsDevice.h"

// インスタンス生成関数
GraphicsDevice& GraphicsDevice::Instance()
{
	static GraphicsDevice instance;
	return instance;
}

void GraphicsDevice::Initialize(HWND _hwnd, int _width, int _height)
{
	// デバッグレイヤーの作成
	/*
		このバリデーションはDevice作成の瞬間に内部のAPIテーブルを差し替えて割り込む
		= Deviceが生まれる時にしか介入するチャンスがない
	*/
#ifdef _DEBUG
	ComPtr<ID3D12Debug> debugController;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
	{
		debugController->EnableDebugLayer();
	}
#endif // _DEBUG

	// Factoryの作成
	UINT dxgiFlags{ 0 }; // デバッグ用
#ifdef _DEBUG
	dxgiFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif // _DEBUG
	HRESULT result{}; // 戻り値確認用
	result = CreateDXGIFactory2(dxgiFlags, IID_PPV_ARGS(&factory));
	if (FAILED(result)) return; // 失敗したら終わる

	// adapter選択とDeviceの作成
	// adapter = どのGPUを使うかの窓口
	ComPtr<IDXGIAdapter1> adapter; // アダプター
	result =  factory->EnumAdapterByGpuPreference(0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter)); // 一番性能の高いGPUを取得する(0番目、性能の高い順)
	if (FAILED(result)) return; // 失敗したら終わる

	// デバイスの作成
	result = D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&device));
	if (FAILED(result)) return; // 失敗したら終わる

	// コマンドキューの作成

}