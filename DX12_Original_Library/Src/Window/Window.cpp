#include "../Debug/DebugLogs.h"
#include "Window.h"

bool Window::GenerateWindow(int _clientWidth, int _clientHeight)
{
	if (_clientWidth <= 0 || _clientHeight <= 0)
	{
		DEBUG_LOG_ERROR("ウィンドウのサイズには0より大きい値を渡してください\n");
		return false;
	}

	// 現在はスワップチェーンなどのリサイズ処理を持っていないため最大化とドラッグによるサイズ変更を禁止する
	constexpr DWORD WINDOW_STYLE{ WS_OVERLAPPEDWINDOW & ~(WS_THICKFRAME | WS_MAXIMIZEBOX) };

	// ウィンドウクラスの設定
	WNDCLASSEX wc{}; // ウィンドウクラス
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.lpfnWndProc = WindowProc; //　メッセージ処理(今はデフォルト)
	wc.hInstance = GetModuleHandle(nullptr);
	wc.lpszClassName = windowName; // クラス名

	const ATOM classAtom{ RegisterClassEx(&wc) };
	// すでに同じクラスが登録されている場合以外の失敗を検出する
	if (classAtom == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
	{
		DEBUG_LOG_ERROR("ウィンドウクラスの登録に失敗しました\n");
		return false;
	}

	// タイトルバーとウィンドウの枠分外側サイズを大きくする
	RECT windowRect{ 0, 0, _clientWidth, _clientHeight };
	if (!AdjustWindowRectEx(&windowRect, WINDOW_STYLE, false, 0))
	{
		DEBUG_LOG_ERROR("ウィンドウサイズの調整に失敗しました\n");
		return false;
	}
	const int windowWidth{ windowRect.right - windowRect.left };
	const int windowHeight{ windowRect.bottom - windowRect.top };

	hwnd = CreateWindowExW(
		0,
	   wc.lpszClassName, // クラス名
	   windowName, // タイトルバー
	   WINDOW_STYLE, // スタイル(標準ウィンドウ)
	   CW_USEDEFAULT, CW_USEDEFAULT, // 位置
	   windowWidth, windowHeight, // サイズ
	   nullptr, nullptr,
	   wc.hInstance,
	   this // マウス回転を積むためにプロシージャに自身のポインタを渡す
   );

	if (!hwnd)
	{
		DEBUG_LOG_ERROR("WindowHandleが空です\n");
		return false;
	}


	ShowWindow(hwnd, SW_SHOW);
	return true;
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
