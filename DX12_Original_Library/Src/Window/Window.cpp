#include "Window.h"

void Window::GenerateWindow()
{
	// ウィンドウクラスの設定
	WNDCLASSEX wc{}; // ウィンドウクラス
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.lpfnWndProc = WindowProc; //　メッセージ処理(今はデフォルト)
	wc.hInstance = GetModuleHandle(nullptr);
	wc.lpszClassName = windowName; // クラス名

	RegisterClassEx(&wc);

	hwnd = CreateWindow(
	   wc.lpszClassName, // クラス名
	   windowName, // タイトルバー
	   WS_OVERLAPPEDWINDOW, // スタイル(標準ウィンドウ)
	   CW_USEDEFAULT, CW_USEDEFAULT, // 位置
	   1280, 720, // サイズ
	   nullptr, nullptr,
	   wc.hInstance,
	   this // マウス回転を積むためにプロシージャに自身のポインタを渡す
   );

	ShowWindow(hwnd, SW_SHOW);
}

// メンバ関数は暗黙的にthisポインタを持つので引数の整合性を取るためにstatic関数にする必要がある
LRESULT CALLBACK Window::WindowProc(HWND _hwnd, UINT _msg, WPARAM _wp, LPARAM _lp)
{
	// 作成時に渡されたthisポインタをウィンドウに紐づける
	if (_msg == WM_NCCREATE)
	{
		CREATESTRUCT* pCreate{ reinterpret_cast<CREATESTRUCT*>(_lp) };
		Window* pWindow{ reinterpret_cast<Window*>(pCreate->lpCreateParams) };
		SetWindowLongPtr(_hwnd, GWLP_USERDATA ,reinterpret_cast<LONG_PTR>(pWindow));
	}

	// ウィンドウに紐づけれられたthisポインタを取得する
	Window* pWindow{ reinterpret_cast<Window*>(GetWindowLongPtr(_hwnd, GWLP_USERDATA)) };

	// インスタンスが取得できている場合のみメンバ処理
	if (pWindow)
	{
		// wheelメッセージの処理
		if (_msg == WM_MOUSEWHEEL)
		{
			if (pWindow->onWheel)
			{
				pWindow->onWheel(GET_WHEEL_DELTA_WPARAM(_wp));
			}
			return 0;
		}
	}

	// 終了処理
	if (_msg == WM_DESTROY)
	{
		PostQuitMessage(0); // WM_QUITをメッセージキューに投げる
		return 0;
	}

	return DefWindowProc(_hwnd, _msg, _wp, _lp);
}