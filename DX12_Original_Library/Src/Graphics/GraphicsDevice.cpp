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

	// 現在のバッファ番号を取得
	currentFrameIndex = swapChain->GetCurrentBackBufferIndex();

	// RTV用ディスクリプタヒープを作成しバックバッファにRTVを作成

	// ディスクリプタヒープを作成
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc{}; // 設定用構造体
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; // RTVを指定
	rtvHeapDesc.NumDescriptors = FRAME_BUFFER_COUNT; // バッファ数分
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE; // RTVがCPUonlyのため(GPU非可視)

	// ディスクリプタヒープの実際の作成
	result = device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvHeap));
	DEBUG_ASSERT(SUCCEEDED(result)); // デバッグ時失敗したら場所を知らせる
	if (FAILED(result)) return;

	// ディスクリプタ一つ分のサイズを取得
	rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV); // RTVなのでそれを指定

	// Heapの先頭ハンドルを取得
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle{ rtvHeap->GetCPUDescriptorHandleForHeapStart()};

	for (int i = 0; i < FRAME_BUFFER_COUNT; i++)
	{
		// バックバッファの取得
		result = swapChain->GetBuffer(i, IID_PPV_ARGS(&backBuffers[i]));
		DEBUG_ASSERT(SUCCEEDED(result)); // デバッグ時失敗したら場所を知らせる
		if (FAILED(result)) return;

		// RTVの作成
		device->CreateRenderTargetView(backBuffers[i].Get(), nullptr, rtvHandle);

		// スロット一つ分ずらす
		rtvHandle.ptr += rtvDescriptorSize;
	}

	// 深度バッファの作成
	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc{};
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV; // 深度バッファなのでDSV指定
	dsvHeapDesc.NumDescriptors = 1; // dsv数。独自に作るのは一つなので1
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE; // GPU非可視

	result = device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&dsvHeap));
	DEBUG_ASSERT(SUCCEEDED(result)); // デバッグ時失敗したら場所を知らせる
	if (FAILED(result)) return;
	
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
	if (FAILED(result)) return;

	// 深度バッファ用ヒープの先頭ハンドルを取得
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle{ dsvHeap->GetCPUDescriptorHandleForHeapStart() };
	device->CreateDepthStencilView(dsvResource.Get(), nullptr, dsvHandle);

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

	// コマンドリストの作成(最初のアロケーターと紐づけて1つだけ)
	result = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmdAllocators[0].Get(), nullptr, IID_PPV_ARGS(&cmdList)); // 初期PSOは後で設定するのでnullptr
	DEBUG_ASSERT(SUCCEEDED(result)); // デバッグ時失敗したら場所を知らせる
	if (FAILED(result)) return;
	// コマンドリストは記録状態で生まれるがBeginFrameの最初にResetから始めたいためCloseしておく
	cmdList->Close(); // 閉じる

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
	// GPUが全処理終了するのを待ってから終了する(リソースが残ったまま開放するとクラッシュする)
	cmdQueue->Signal(fence.Get(), ++fenceValueCounter);
	if (fence->GetCompletedValue() < fenceValueCounter)
	{
		HANDLE event{ CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS) };
		DEBUG_ASSERT(event != nullptr); // デバッグ時失敗したら場所を知らせる
		if (!event) return; // nullチェック
		fence->SetEventOnCompletion(fenceValueCounter, event);
		WaitForSingleObject(event, INFINITE);
		CloseHandle(event);
	}
}

bool GraphicsDevice::WaitForGPU()
{
	// コマンドキューとフェンスガン変えればそもそも失敗
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
		if (!event) return; // nullチェック
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
void GraphicsDevice::EndFrame()
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
	cmdList->Close();

	// コマンドリストをコマンドキューに投げる
	ID3D12CommandList* cmdLists[]{ cmdList.Get() }; // 複数のコマンドリストを投げられるよう配列管理する
	cmdQueue->ExecuteCommandLists(1, cmdLists);

	// バッファ交換を行う(Present)
	swapChain->Present(1, 0); // 第一引数 : VSyncの間隔(1 = 60fps同期) 今後この戻り値はassert候補

	// フェンスシグナルを出す(このフレームの命令が全て終わったらカウンタをこの値にしろという命令)
	fenceValues[currentFrameIndex] = ++fenceValueCounter;
	cmdQueue->Signal(fence.Get(), fenceValues[currentFrameIndex]);

	// 現在のframeIndexを更新
	currentFrameIndex = swapChain->GetCurrentBackBufferIndex();
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

	// 結果用変数
	HRESULT result{};

	// AllocatorReser
	result = cmdAllocators[currentFrameIndex]->Reset();
	DEBUG_ASSERT(SUCCEEDED(result));
	if (FAILED(result))
	{
		// 失敗はそのまま返す
		return result;
	} 

	// CommandListを開く
	result = cmdList->Reset(cmdAllocators[currentFrameIndex].Get(), nullptr);
	DEBUG_ASSERT(SUCCEEDED(result));
	if (FAILED(result))
	{
		return result;
	}

	// 呼び出し側にコピー命令とバリアを積ませる
	_recode(cmdList.Get());

	// commandListを閉じる
	result = cmdList->Close();
	DEBUG_ASSERT(SUCCEEDED(result));
	if (FAILED(result))
	{
		return result;
	}

	// GPUに投げる
	ID3D12CommandList* commandLists[]{ cmdList.Get() };
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