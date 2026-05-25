#include "GraphicsDevice.h"
#include "../Src/Graphics/GraphicsConstant.h"
#include "DescriptorManager.h"
#include "ShaderSystem.h"
#include "DrawDebug/DebugTriangle.h"

// カスタムのウィンドウプロシージャ
LRESULT CALLBACK WindowProc(HWND _hwnd, UINT _msg, WPARAM _wp, LPARAM _lp)
{
	if (_msg == WM_DESTROY)
	{
		PostQuitMessage(0); // WM_QUITをメッセージキューに投げる
		return 0;
	}
	return DefWindowProc(_hwnd, _msg, _wp, _lp);
}

// エントリーポイント
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	// ウィンドウクラスの設定
	WNDCLASSEX wc{}; // ウィンドウクラス
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.lpfnWndProc = WindowProc; //　メッセージ処理(今はデフォルト)
	wc.hInstance = GetModuleHandle(nullptr);
	wc.lpszClassName = L"GraphicsTest"; // クラス名

	RegisterClassEx(&wc);

	HWND hwnd{ CreateWindow(
		wc.lpszClassName, // クラス名
		L"Graphics Test", // タイトルバー
		WS_OVERLAPPEDWINDOW, // スタイル(標準ウィンドウ)
		CW_USEDEFAULT, CW_USEDEFAULT, // 位置
		1280, 720, // サイズ
		nullptr, nullptr,
		wc.hInstance,
		nullptr
	) };

	ShowWindow(hwnd, SW_SHOW);

	GraphicsDevice::Instance().Initialize(hwnd, 1280, 720); // 初期化
	DescriptorManager::Instance().Initialize(GraphicsDevice::Instance().GetDevice()); // ディスクリプタマネージャーをデバイスを使って初期化

	DescriptorHandle h1{ DescriptorManager::Instance().Allocate(HeapType::CBV_SRV_UAV) }; // GPU可視
	DescriptorHandle h2{ DescriptorManager::Instance().Allocate(HeapType::CBV_SRV_UAV) }; // GPU可視

	// h1とh2が別のインデックスであることを確認する
	if (h1.index != h2.index)
	{
		OutputDebugStringA("[PASS] : インデックスが異なる値を出力できています");
	}
	else
	{
		OutputDebugStringA("[FAIL] : インデックスが同じ値を出力しています");
	}

	// Freeして再度Allocateすると同じインデックスが戻るか
	DescriptorManager::Instance().Free(HeapType::CBV_SRV_UAV, h2);
	DescriptorHandle h3{ DescriptorManager::Instance().Allocate(HeapType::CBV_SRV_UAV) }; // 再度取得

	if (h2.index == h3.index)
	{
		OutputDebugStringA("[PASS] : 一度戻した後も同じインデックスが返っています");
	}
	else
	{
		OutputDebugStringA("[FAIL] : 一度戻した後違うインデックスが返っています");
	}

	ShaderSystem shaderSystem;
	shaderSystem.Initialize(GraphicsDevice::Instance().GetDevice());

	// シェーダーのコンパイル
	auto vsBlob{shaderSystem.Compile(L"../Src/Shaders/TriangleVS.hlsl", "main", "vs_5_0")};
	auto psBlob{shaderSystem.Compile(L"../Src/Shaders/TrianglePS.hlsl", "main", "ps_5_0")};

	auto rootSignature{shaderSystem.CreateRootSignature()}; // ルートシグネチャの作成

	auto pipeLineState{ shaderSystem.CreatePipeLineState(rootSignature.Get(), vsBlob.Get(), psBlob.Get()) }; // パイプラインステートオブジェクトを作成

	DebugTriangle triangle{};
	triangle.Initialize();

	MSG msg{};
	while (msg.message != WM_QUIT)
	{
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			DispatchMessage(&msg);
		}
		else
		{
			// 描画
			GraphicsDevice::Instance().BeginFrame(); // フレームの最初の処理

			// 画面色をクリアする
			float windowColor[]{ 0.1f, 0.3f, 0.3f, 1.0f }; // 任意の色
			auto rtv{ GraphicsDevice::Instance().GetCurrentRTV() }; // 現在のRTVハンドルを取得
			GraphicsDevice::Instance().GetCommandList()->ClearRenderTargetView(rtv, windowColor, 0 , nullptr); // コマンドリストを取得しそこから現在書き込んでいるRTVにの色を任意色でクリアする

			// 三角形描画処理

			auto commandList{ GraphicsDevice::Instance().GetCommandList() }; // コマンドリストキャッシュ

			// ビューポート
			D3D12_VIEWPORT viewPort{};
			viewPort.TopLeftX = 0.0f;
			viewPort.TopLeftY = 0.0f;
			viewPort.Width = 1280.0f; // テスト用に一時的に
			viewPort.Height = 720.f; // テスト用に一時的に
			viewPort.MinDepth = 0.0f;
			viewPort.MaxDepth = 1.0f;

			// シザー矩形
			D3D12_RECT scissorRect{};
			scissorRect.left = 0;
			scissorRect.top = 0;
			scissorRect.right = 1280;
			scissorRect.bottom = 720;

			commandList->RSSetViewports(1, &viewPort);
			commandList->RSSetScissorRects(1, &scissorRect);

			commandList->OMSetRenderTargets(1, &rtv, false, nullptr);

			commandList->ClearRenderTargetView(rtv, windowColor, 0, nullptr);

			commandList->SetGraphicsRootSignature(rootSignature.Get());
			commandList->SetPipelineState(pipeLineState.Get());

			triangle.Draw(commandList);

			GraphicsDevice::Instance().EndFrame(); // フレームの最後の処理
		}
	}
	DescriptorManager::Instance().Shutdown();
	GraphicsDevice::Instance().Shutdown();
	return 0;
}