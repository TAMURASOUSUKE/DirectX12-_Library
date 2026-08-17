#include "../Debug/DebugLogs.h"
#include "GraphicsDevice.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

// インスタンス生成関数
GraphicsDevice& GraphicsDevice::Instance()
{
	static GraphicsDevice instance;
	return instance;
}

void GraphicsDevice::Setup(HWND _hwnd, int _width, int _height)
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
	DEBUG_ASSERT(SUCCEEDED(result)); // デバッグ時失敗したら場所を知らせる
	if (FAILED(result)) return; // Release時でも失敗したら終わる

	// adapter選択とDeviceの作成
	// adapter = どのGPUを使うかの窓口
	ComPtr<IDXGIAdapter1> adapter; // アダプター
	result =  factory->EnumAdapterByGpuPreference(0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter)); // 一番性能の高いGPUを取得する(0番目、性能の高い順)
	DEBUG_ASSERT(SUCCEEDED(result)); // デバッグ時失敗したら場所を知らせる
	if (FAILED(result)) return; // 失敗したら終わる

	// デバイスの作成
	result = D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&device));
//#ifdef _DEBUG
//	ComPtr<ID3D12InfoQueue> infoQueue;
//	if (SUCCEEDED(device.As(&infoQueue)))
//	{
//		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
//		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
//	}
//#endif
	DEBUG_ASSERT(SUCCEEDED(result)); // デバッグ時失敗したら場所を知らせる
	if (FAILED(result)) return; // 失敗したら終わる

	// コマンドキューの作成
	D3D12_COMMAND_QUEUE_DESC queueDesc{}; // コマンドキューの設定用構造体
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT; // 描画命令(DIRECT = 描画もコンピュートもコピーもできる汎用型)
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE; // タイムアウトなし

	// 実際の作成
	result = device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&cmdQueue));
	DEBUG_ASSERT(SUCCEEDED(result)); // デバッグ時失敗したら場所を知らせる
	if (FAILED(result)) return;

	// スワップチェーンの作成
	DXGI_SWAP_CHAIN_DESC1 scDesc{}; // スワップチェーン設定用構造体
	scDesc.Width = _width; // 横幅
	scDesc.Height = _height; // 縦幅
	scDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // 各色8bit、0-1正規化
	scDesc.SampleDesc = { 1, 0 }; // MSAAはなし
	scDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; // 描画先として使う
	scDesc.BufferCount = FRAME_BUFFER_COUNT; // ダブルバッファ
	scDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; // 前フレームの内容を再利用する必要はないのでこれ(表示が終わったバッファの中身は保証しないという意味)

	// スワップチェーンの作成関数はIDXGISwapChain1を返すがメンバは4のためAsで変換する
	ComPtr<IDXGISwapChain1> swapChain1{};
	result = factory->CreateSwapChainForHwnd(cmdQueue.Get(), _hwnd, &scDesc, nullptr, nullptr, &swapChain1);
	DEBUG_ASSERT(SUCCEEDED(result)); // デバッグ時失敗したら場所を知らせる
	if (FAILED(result)) return;

	// 4型にcast
	result = swapChain1.As(&swapChain);
	DEBUG_ASSERT(SUCCEEDED(result)); // デバッグ時失敗したら場所を知らせる
	if (FAILED(result)) return;

	// backbuffer取得からRTVの作成
	if (!CreateBackBufferView()) return;
	// 深度バッファの作成
	if (!CreateDepthStencilBuffer(static_cast<UINT>(_width), static_cast<UINT>(_height))) return;

	// コマンドアロケーターとコマンドリストを作る
	for (int i = 0; i < FRAME_BUFFER_COUNT; i++)
	{
		/*
			フレームごと交互に書き込みと実行ができるようにアロケーターは2つ用意する
		*/
		result = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&cmdAllocators[i]));
		DEBUG_ASSERT(SUCCEEDED(result)); // デバッグ時失敗したら場所を知らせる
		if (FAILED(result)) return;
	}

	// リソース転送専用のコマンドアロケーター
	result = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&uploadCmdAllocator));
	DEBUG_ASSERT(SUCCEEDED(result));
	if (FAILED(result)) return;

	// コマンドリストの作成(最初のアロケーターと紐づけて1つだけ)
	result = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmdAllocators[0].Get(), nullptr, IID_PPV_ARGS(&cmdList)); // 初期PSOは後で設定するのでnullptr
	DEBUG_ASSERT(SUCCEEDED(result)); // デバッグ時失敗したら場所を知らせる
	if (FAILED(result)) return;
	// コマンドリストは記録状態で生まれるがBeginFrameの最初にResetから始めたいためCloseしておく
	result = cmdList->Close(); // 閉じる
	DEBUG_ASSERT(SUCCEEDED(result)); // デバッグ時失敗したら場所を知らせる
	if (FAILED(result)) return;

	// リソース転送専用のコマンドリストを作る
	result = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, uploadCmdAllocator.Get(), nullptr, IID_PPV_ARGS(&uploadCmdList));
	DEBUG_ASSERT(SUCCEEDED(result));
	if (FAILED(result)) return;
	// 作成直後は記録状態なので最初のResetようにCloseする
	result = uploadCmdList->Close();
	DEBUG_ASSERT(SUCCEEDED(result)); // デバッグ時失敗したら場所を知らせる
	if (FAILED(result)) return;

	// fenceValueは0初期化
	for (int i = 0; i < FRAME_BUFFER_COUNT; i++)
	{
		fenceValues[i] = 0;
	}
	// フェンスの作成
	result = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
	DEBUG_ASSERT(SUCCEEDED(result)); // デバッグ時失敗したら場所を知らせる
	if (FAILED(result)) return;
}

// 終了処理
void GraphicsDevice::Shutdown()
{
	uploadCmdList.Reset();
	uploadCmdAllocator.Reset();
	cmdList.Reset();
	for (ComPtr<ID3D12CommandAllocator>& allocator : cmdAllocators)
	{
		allocator.Reset();
	}

	for (ComPtr<ID3D12Resource>& backBuffer : backBuffers)
	{
		backBuffer.Reset();
	}

	dsvResource.Reset();
	rtvHeap.Reset();
	dsvHeap.Reset();
	swapChain.Reset();
	fence.Reset();
	cmdQueue.Reset();
	device.Reset();
}

bool GraphicsDevice::WaitForGPU()
{
	// コマンドキューとフェンスがなければそもそも失敗
	if (!cmdQueue || !fence)
	{
		return false;
	}
	const UINT64 waitValue{ ++fenceValueCounter };
	HRESULT result{ cmdQueue->Signal(fence.Get(), waitValue) };  // フェンス値の設定
	if (FAILED(result))
	{
		DEBUG_LOG_ERROR("フェンス値の設定に失敗しました\n");
		return false;
	}

	// 設定したフェンス値になっているか設定
	if (fence->GetCompletedValue() < waitValue)
	{
		HANDLE event{ CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS) };
		DEBUG_ASSERT(event != nullptr); // デバッグ時失敗したら場所を知らせる
		if (!event) return false; // nullチェック
		// フェンスの値が第一引数以上になるとeventがシグナル状態になる
		fence->SetEventOnCompletion(waitValue, event);

		// eventがシグナル状態になるまで待機
		const DWORD waitResult{ WaitForSingleObject(event, INFINITE) };
		CloseHandle(event); // eventを閉じる
		if (waitResult != WAIT_OBJECT_0)
		{
			DEBUG_LOG_ERROR("GPU待機処理が失敗しました\n");
			return false;
		}
	}


	return true;
}

// フレームの最初に行う処理
void GraphicsDevice::BeginFrame()
{
	// 一つ前のフレームのアロケーターをリセットする際に実行中か確認する必要があるためフェンスを用意
	// GPUがこのフレームの処理を終えているのか確認
	if (fence->GetCompletedValue() < fenceValues[currentFrameIndex]) // フェンス値が返ってくるので
	{
		// まだ終わっていない
		HANDLE event{ CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS) }; // イベントの作成
		if (!event) return; // nullチェック
		fence->SetEventOnCompletion(fenceValues[currentFrameIndex], event); // フェンス値がこの値に達したらイベントを発火しろという命令
		WaitForSingleObject(event, INFINITE); // イベントを待つ(INFINIT = 待ち続ける)
		CloseHandle(event); // イベントをしまう
	}

	// アロケーターをリセット
	cmdAllocators[currentFrameIndex]->Reset();

	// コマンドリストをリセット(アロケーターと紐づけなおす)
	cmdList->Reset(cmdAllocators[currentFrameIndex].Get(), nullptr);

	// リソースバリアの作成(表示用から描画先への切り替え)
	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = backBuffers[currentFrameIndex].Get();
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT; // 表示用
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET; // 描画先
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	cmdList->ResourceBarrier(1, &barrier);
}

// フレームの最後に行う処理
bool GraphicsDevice::EndFrame()
{
	// 描画先-> 表示用
	D3D12_RESOURCE_BARRIER barrier{}; // バリア
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = backBuffers[currentFrameIndex].Get();
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET; // 描画先
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT; // 表示用
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	cmdList->ResourceBarrier(1, &barrier);

	// コマンドリストを閉じる(これ以上の書き込みを禁止する)
	const HRESULT closeResult{ cmdList->Close() };
	if (FAILED(closeResult))
	{
		DEBUG_LOG_ERROR("描画コマンドリストのCloseに失敗しました HRESULT=0x{:08X}\n", static_cast<unsigned int>(closeResult));
		DEBUG_ASSERT(false);
		return false;
	}

	// コマンドリストをコマンドキューに投げる
	ID3D12CommandList* cmdLists[]{ cmdList.Get() }; // 複数のコマンドリストを投げられるよう配列管理する
	cmdQueue->ExecuteCommandLists(1, cmdLists);

	const UINT vsync{ isVSyncEnabled ? 1u : 0u };

	// バッファ交換を行う(Present)
	const HRESULT presentResult{ swapChain->Present(vsync, 0) }; // 第一引数 : VSyncの間隔(1 = モニターの垂直同期を一回待つ)
	if (FAILED(presentResult))
	{
		const HRESULT removedReason{ device ? device->GetDeviceRemovedReason() : E_POINTER };
		DEBUG_LOG_ERROR("Presentに失敗しました HRESULT=0x{:08X} DeviceRemovedReason=0x{:08X}\n", static_cast<unsigned int>(presentResult), static_cast<unsigned int>(removedReason));
		// 描画命令はすでにExcuteしているのでGPU作業追跡のためSignalまでは行う
	}
	// まだ成功していない値を先に確定値へ入れない
	const UINT64 signalValue{ fenceValueCounter + 1 };
	const UINT submittedFrameIndex{ currentFrameIndex };
	// フェンスシグナルを出す(このフレームの命令が全て終わったらカウンタをこの値にしろという命令)
	const HRESULT signalResult{ cmdQueue->Signal(fence.Get(), signalValue) };
	if (FAILED(signalResult))
	{
		DEBUG_LOG_ERROR("フレーム終了時のFence Signalに失敗しました HRESULT=0x{:08X}\n", static_cast<unsigned int>(signalResult));
		DEBUG_ASSERT(false);
		return false;
	}

	// シグナルが成功してから最後に送信した値として確定する
	fenceValueCounter = signalValue;
	fenceValues[submittedFrameIndex] = signalValue;
	
	if (FAILED(presentResult))
	{
		DEBUG_ASSERT(false);
		return false;
	}

	// 現在のframeIndexを更新
	currentFrameIndex = swapChain->GetCurrentBackBufferIndex();
	return true;
}

bool GraphicsDevice::Resize(UINT _width, UINT _height)
{
	// 最小化時には0x0がくるため受け入れない
	if (_width == 0 || _height == 0) return false;
	if (!swapChain || !device) return false;

	// GPUが古いBackBufferを使い終わるまで待つ
	if (!WaitForGPU()) return false;

	// Resize前にSwapChainから取得した参照を解放する
	for (auto& backBuffer : backBuffers)
	{
		backBuffer.Reset();
	}
	dsvResource.Reset();
	const HRESULT result{ swapChain->ResizeBuffers(FRAME_BUFFER_COUNT, _width, _height,  DXGI_FORMAT_R8G8B8A8_UNORM, 0) };
	if (FAILED(result))
	{
		DEBUG_LOG_ERROR("SwapChainのResizeBuffersに失敗しました HRESULT=0x{:08X}\n", static_cast<unsigned int>(result));
		return false;
	}

	currentFrameIndex = swapChain->GetCurrentBackBufferIndex();
	if (!CreateBackBufferView()) return false; // BackBufferViewの作成
	if (!CreateDepthStencilBuffer(_width, _height)) return false; // 深度バッファの作成
	return true;
}


ID3D12Device* GraphicsDevice::GetDevice() const
{
	return device.Get();
}

ID3D12GraphicsCommandList* GraphicsDevice::GetCommandList() const
{
	return cmdList.Get();
}

D3D12_CPU_DESCRIPTOR_HANDLE GraphicsDevice::GetCurrentRTV() const
{
	D3D12_CPU_DESCRIPTOR_HANDLE handle{ rtvHeap->GetCPUDescriptorHandleForHeapStart() }; // 先頭ハンドル
	handle.ptr += currentFrameIndex * rtvDescriptorSize;
	return handle;
}

D3D12_CPU_DESCRIPTOR_HANDLE GraphicsDevice::GetDSV() const
{
	D3D12_CPU_DESCRIPTOR_HANDLE handle{ dsvHeap->GetCPUDescriptorHandleForHeapStart() }; // 先頭ハンドル
	return handle;
}

UINT GraphicsDevice::GetCurrentFrameIndex() const
{
	return currentFrameIndex;
}

// ヘルパー
HRESULT GraphicsDevice::ExecuteUpdate(std::function<void(ID3D12GraphicsCommandList*)> _recode)
{
	// nullかチェックする
	DEBUG_ASSERT(_recode != nullptr);
	if (!_recode)
	{
		return E_INVALIDARG;
	}

	if (!uploadCmdAllocator || !uploadCmdList || !cmdQueue || !fence)
	{
		DEBUG_LOG_ERROR("アップロード用のDirectX12オブジェクトが初期化されていません\n");
		return E_FAIL;
	}

	// 結果用変数
	HRESULT result{};

	// AllocatorReser
	result = uploadCmdAllocator->Reset();
	DEBUG_ASSERT(SUCCEEDED(result));
	if (FAILED(result)) return result;

	// CommandListを開く
	result = uploadCmdList->Reset(uploadCmdAllocator.Get(), nullptr);
	DEBUG_ASSERT(SUCCEEDED(result));
	if (FAILED(result)) return result;

	// 呼び出し側にコピー命令とバリアを積ませる
	_recode(uploadCmdList.Get());

	// commandListを閉じる
	result = uploadCmdList->Close();
	DEBUG_ASSERT(SUCCEEDED(result));
	if (FAILED(result)) return result;

	// GPUに投げる
	ID3D12CommandList* commandLists[]{ uploadCmdList.Get() };
	cmdQueue->ExecuteCommandLists(1, commandLists);

	// この単発アップロード命令の完了フェンス値
	const UINT64 uploadFenceValue{ ++fenceValueCounter };

	result = cmdQueue->Signal(fence.Get(), uploadFenceValue);
	DEBUG_ASSERT(SUCCEEDED(result));
	if (FAILED(result))
	{
		return result;
	}

	// 即時待機
	if (fence->GetCompletedValue() < uploadFenceValue)
	{
		HANDLE event{ CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS) }; // 待機用のイベントオブジェクト
		if (!event)
		{
			DEBUG_LOG_WARNING("イベント作成に失敗しました\n");
			return HRESULT_FROM_WIN32(GetLastError()); // WindowsのエラーをHRESULTに変換
		}

		// フェンス値に到達するかチェック
		result = fence->SetEventOnCompletion(uploadFenceValue, event);
		if (FAILED(result))
		{
			DEBUG_LOG_WARNING("登録に失敗しました\n");
			CloseHandle(event);
			return result;
		}

		DWORD waitResult{ WaitForSingleObject(event, INFINITE) }; // CPU待機
		CloseHandle(event); // 待ち終わったら閉じる

		// 待機チェック
		if (waitResult != WAIT_OBJECT_0)
		{
			DEBUG_LOG_WARNING("待機が成功しませんでした\n");
			return HRESULT_FROM_WIN32(GetLastError());
		}
	}

	return S_OK; // ここまで来たら成功を返す
}

UINT64 GraphicsDevice::GetCompletedFenceValue() const
{
	// Shutdown後など、フェンスが存在しない場合の保険
	if (fence == nullptr)
	{
		return 0;
	}

	// GPUが実際に処理を完了したフェンス値
	return fence->GetCompletedValue();
}

UINT64 GraphicsDevice::GetLastSubmittedFenceValue() const
{
	// CPU側が最後にSignalへ使用したフェンス値
	return fenceValueCounter;
}

bool GraphicsDevice::CreateBackBufferView()
{
	HRESULT result{};

	// RTV用ディスクリプタヒープを作成しバックバッファにRTVを作成
	if (!rtvHeap) // Resize時に作り直さなくていいようにする
	{
		// 現在のバッファ番号を取得
		currentFrameIndex = swapChain->GetCurrentBackBufferIndex();

		// ディスクリプタヒープを作成
		D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc{}; // 設定用構造体
		rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; // RTVを指定
		rtvHeapDesc.NumDescriptors = FRAME_BUFFER_COUNT; // バッファ数分
		rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE; // RTVがCPUonlyのため(GPU非可視)

		// ディスクリプタヒープの実際の作成
		result = device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvHeap));
		DEBUG_ASSERT(SUCCEEDED(result)); // デバッグ時失敗したら場所を知らせる
		if (FAILED(result)) return false;

		// ディスクリプタ一つ分のサイズを取得
		rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV); // RTVなのでそれを指定
	}

	// Heapの先頭ハンドルを取得
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle{ rtvHeap->GetCPUDescriptorHandleForHeapStart() };

	// RenderTargetViewの設定構造体を用いて設定を行う
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Texture2D.MipSlice = 0;
	rtvDesc.Texture2D.PlaneSlice = 0;

	for (int i = 0; i < FRAME_BUFFER_COUNT; i++)
	{
		// バックバッファの取得
		result = swapChain->GetBuffer(i, IID_PPV_ARGS(&backBuffers[i]));
		DEBUG_ASSERT(SUCCEEDED(result)); // デバッグ時失敗したら場所を知らせる
		if (FAILED(result)) return false;

		// RTVの作成
		device->CreateRenderTargetView(backBuffers[i].Get(), &rtvDesc, rtvHandle);

		// スロット一つ分ずらす
		rtvHandle.ptr += rtvDescriptorSize;
	}
	return true;
}

bool GraphicsDevice::CreateDepthStencilBuffer(UINT _width, UINT _height)
{
	HRESULT result{};
	if (!dsvHeap) // Resize時に作り直さなくていいようにする
	{
		// 深度バッファの作成
		D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc{};
		dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV; // 深度バッファなのでDSV指定
		dsvHeapDesc.NumDescriptors = 1; // dsv数。独自に作るのは一つなので1
		dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE; // GPU非可視

		result = device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&dsvHeap));
		DEBUG_ASSERT(SUCCEEDED(result)); // デバッグ時失敗したら場所を知らせる
		if (FAILED(result)) return false;
	}

	D3D12_HEAP_PROPERTIES dsvHeapProperties{}; // 頂点ヒープの設定
	dsvHeapProperties.Type = D3D12_HEAP_TYPE_DEFAULT; // デフォルトヒープに設定
	dsvHeapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN; // ページング

	// 深度バッファのリソース設定
	D3D12_RESOURCE_DESC dsvResourceDesc{};
	dsvResourceDesc.Width = _width;
	dsvResourceDesc.Height = _height;
	dsvResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	dsvResourceDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; // 深度24bitステンシル8bit
	dsvResourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL; // 深度バッファとして使う
	dsvResourceDesc.DepthOrArraySize = 1;
	dsvResourceDesc.MipLevels = 1;
	dsvResourceDesc.SampleDesc = { 1, 0 };

	// 深度クリアのための設定
	D3D12_CLEAR_VALUE dsvClearValue{};
	dsvClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	dsvClearValue.DepthStencil.Depth = 1.0f;
	dsvClearValue.DepthStencil.Stencil = 0;

	result = device->CreateCommittedResource(&dsvHeapProperties, D3D12_HEAP_FLAG_NONE, &dsvResourceDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &dsvClearValue, IID_PPV_ARGS(&dsvResource)); // 書き込みかつ深度クリアを入れる
	DEBUG_ASSERT(SUCCEEDED(result)); // デバッグ時失敗したら場所を知らせる
	if (FAILED(result)) return false;

	// 深度バッファ用ヒープの先頭ハンドルを取得
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle{ dsvHeap->GetCPUDescriptorHandleForHeapStart() };
	device->CreateDepthStencilView(dsvResource.Get(), nullptr, dsvHandle);

	return true;
}
