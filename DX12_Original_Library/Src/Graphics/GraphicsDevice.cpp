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
	D3D12_COMMAND_QUEUE_DESC queueDesc{}; // コマンドキューの設定用構造体
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT; // 描画命令(DIRECT = 描画もコンピュートもコピーもできる汎用型)
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE; // タイムアウトなし

	// 実際の作成
	result = device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&cmdQueue));
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
	if (FAILED(result)) return;

	// 4型にcast
	swapChain1.As(&swapChain);
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
	if (FAILED(result)) return;

	// ディスクリプタ一つ分のサイズを取得
	rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV); // RTVなのでそれを指定

	// Heapの先頭ハンドルを取得
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle{ rtvHeap->GetCPUDescriptorHandleForHeapStart()};

	for (int i = 0; i < FRAME_BUFFER_COUNT; i++)
	{
		// バックバッファの取得
		result = swapChain->GetBuffer(i, IID_PPV_ARGS(&backBuffers[i]));
		if (FAILED(result)) return;

		// RTVの作成
		device->CreateRenderTargetView(backBuffers[i].Get(), nullptr, rtvHandle);

		// スロット一つ分ずらす
		rtvHandle.ptr += rtvDescriptorSize;
	}

	// コマンドアロケーターとコマンドリストを作る
	for (int i = 0; i < FRAME_BUFFER_COUNT; i++)
	{
		/*
			フレームごと交互に書き込みと実行ができるようにアロケーターは2つ用意する
		*/
		result = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&cmdAllocators[i]));
		if (FAILED(result)) return;
	}

	// コマンドリストの作成(最初のアロケーターと紐づけて1つだけ)
	result = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmdAllocators[0].Get(), nullptr, IID_PPV_ARGS(&cmdList)); // 初期PSOは後で設定するのでnullptr
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
		if (!event) return; // nullチェック
		fence->SetEventOnCompletion(fenceValueCounter, event);
		WaitForSingleObject(event, INFINITE);
		CloseHandle(event);
	}
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
	swapChain->Present(1, 0); // 第一引数 : VSyncの間隔(1 = 60fps同期)

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