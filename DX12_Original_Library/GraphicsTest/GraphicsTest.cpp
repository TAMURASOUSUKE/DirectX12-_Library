#include "GraphicsDevice.h"

// エントリーポイント
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	// ウィンドウクラスの設定
	WNDCLASSEX wc{}; // ウィンドウクラス
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.lpfnWndProc = DefWindowProc; //　メッセージ処理(今はデフォルト)
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
			GraphicsDevice::Instance().EndFrame(); // フレームの最後の処理
		}
	}

	GraphicsDevice::Instance().Shutdown();
	return 0;
}