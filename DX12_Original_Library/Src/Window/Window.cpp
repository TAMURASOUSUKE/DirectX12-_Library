#include "../Debug/DebugLogs.h"
#include "Window.h"

namespace
{
	// OSへ登録するWindowClassの識別名
	// 表示タイトルとは別物なので固定
	constexpr wchar_t WINDOW_CLASS_NAME[]{ L"TSGameLibWindowClass" };
}

bool Window::GenerateWindow(int _clientWidth, int _clientHeight)
{
	if (_clientWidth <= 0 || _clientHeight <= 0)
	{
		DEBUG_LOG_ERROR("ウィンドウのサイズには0より大きい値を渡してください\n");
		return false;
	}

	if (hwnd != nullptr)
	{
		DEBUG_LOG_ERROR("ウィンドウはすでに生成されています\n");
		return false;
	}

	// 現在はスワップチェーンなどのリサイズ処理を持っていないため最大化とドラッグによるサイズ変更を禁止する
	constexpr DWORD WINDOW_STYLE{ WS_OVERLAPPEDWINDOW & ~(WS_THICKFRAME | WS_MAXIMIZEBOX) };

	// タイトルバーとウィンドウの枠分外側サイズを大きくする
	RECT windowRect{ 0, 0, _clientWidth, _clientHeight };
	if (!AdjustWindowRectEx(&windowRect, WINDOW_STYLE, false, 0))
	{
		DEBUG_LOG_ERROR("ウィンドウサイズの調整に失敗しました\n");
		return false;
	}
	const int windowWidth{ windowRect.right - windowRect.left };
	const int windowHeight{ windowRect.bottom - windowRect.top };

	return GenerateNativeWindow(WINDOW_STYLE, CW_USEDEFAULT, CW_USEDEFAULT, windowWidth, windowHeight);
}

bool Window::GenerateBorderlessFullscreen()
{
	// 現在はプライマリモニターを使用する
	const POINT primaryMonitorPoint{ 0, 0 };
	const HMONITOR monitor{ MonitorFromPoint(primaryMonitorPoint, MONITOR_DEFAULTTOPRIMARY) };

	if (monitor == nullptr)
	{
		DEBUG_LOG_ERROR("プライマリモニターの取得に失敗しました\n");
		return false;
	}

	MONITORINFO monitorInfo{};
	monitorInfo.cbSize = sizeof(MONITORINFO);

	if (!GetMonitorInfoW(monitor, &monitorInfo))
	{
		DEBUG_LOG_ERROR("モニター情報の取得に失敗しました\n");
		return false;
	}

	const RECT& monitorRect{ monitorInfo.rcMonitor };

	const int monitorWidth{ monitorRect.right - monitorRect.left };
	const int monitorHeight{ monitorRect.bottom - monitorRect.top };

	// WS_POPUPにはタイトルバーや外枠がない
	constexpr DWORD BORDERLESS_STYLE{ WS_POPUP };

	return GenerateNativeWindow(BORDERLESS_STYLE, monitorRect.left, monitorRect.top, monitorWidth, monitorHeight);
}

void Window::Shutdown()
{
	// Inputへのコールバック参照を先に切る
	onWheel = {};
	if (hwnd && IsWindow(hwnd)) DestroyWindow(hwnd);
	hwnd = nullptr;
	// このライブラリが登録したWindowClassを解除する
	const HINSTANCE instance{ GetModuleHandleW(nullptr) };
	if (!UnregisterClassW(WINDOW_CLASS_NAME, instance))
	{
		const DWORD error{ GetLastError() };
		// すでに解除済みなら異常扱いにしない
		if (error != ERROR_CLASS_DOES_NOT_EXIST) DEBUG_LOG_ERROR("ウィンドウクラスの登録解除に失敗しました\n");
	}
}

bool Window::SetWindowTitle(const wchar_t* _title)
{
	if (!_title)
	{
		DEBUG_LOG_ERROR("ウィンドウタイトルにnullptrが渡されました\n");
		return false;
	}

	// 渡された文字列を自分のwstringへコピーする
	windowTitle = _title;
	// Window生成前ならここで返す(保存のみ)
	if (!hwnd) return true;

	// 生成済みなら反映を行う
	if (!SetWindowTextW(hwnd, windowTitle.c_str()))
	{
		DEBUG_LOG_ERROR("ウィンドウタイトルの変更に失敗しました\n");
		return false;
	}
	return true;
}

Vector2Int Window::GetClientSize() const
{
	// ウィンドウハンドルがないならゼロを返す
	if (!hwnd) return Vector2Int::Zero;

	RECT clientRect{};
	if (!GetClientRect(hwnd, &clientRect))
	{
		DEBUG_LOG_ERROR("クライアント領域に取得に失敗しました\n");
		return Vector2Int::Zero;
	}

	return{ clientRect.right - clientRect.left, clientRect.bottom - clientRect.top };
}

bool Window::IsFocused() const
{
	// 単一なのでこの形
	if (hwnd == nullptr) return false;

	return GetForegroundWindow() == hwnd;
}

void Window::RequestQuit()
{
	if (hwnd == nullptr || !IsWindow(hwnd)) return;
	// ×ボタンと同じWM_CLOSEを送る
	if (!PostMessage(hwnd, WM_CLOSE, 0, 0))
	{
		DEBUG_LOG_ERROR("ウィンドウ終了要求の送信に失敗しました\n");
	}
}

bool Window::GenerateNativeWindow(DWORD _windowStyle, int _x, int _y, int _width, int _height)
{
	if (hwnd)
	{
		DEBUG_LOG_ERROR("ウィンドウはすでに生成されています\n");
		return false;
	}

	WNDCLASSEXW windowClass{};
	windowClass.cbSize = sizeof(WNDCLASSEXW);
	windowClass.lpfnWndProc = WindowProc;
	windowClass.hInstance = GetModuleHandleW(nullptr);
	windowClass.lpszClassName = WINDOW_CLASS_NAME;

	// 通常の矢印カーソルを使用する
	windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
	const ATOM classAtom{ RegisterClassExW(&windowClass) };

	// すでに同じクラスが登録されている場合以外の失敗を検出する
	if (classAtom == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
	{
		DEBUG_LOG_ERROR("ウィンドウクラスの登録に失敗しました\n");
		return false;
	}

	hwnd = CreateWindowExW(
		0,
		WINDOW_CLASS_NAME, // クラス名
		windowTitle.c_str(), // タイトルバー
		_windowStyle, // スタイル
		_x, _y, // 位置
		_width, _height, // サイズ
		nullptr, nullptr,
		windowClass.hInstance,
		this // マウス回転を積むためにプロシージャに自身のポインタを渡す
	);

	if (!hwnd)
	{
		DEBUG_LOG_ERROR("WindowHandleの生成に失敗しました\n");
		return false;
	}


	ShowWindow(hwnd, SW_SHOW);
	UpdateWindow(hwnd);
	return true;
}

// メンバ関数は暗黙的にthisポインタを持つので引数の整合性を取るためにstatic関数にする必要がある
LRESULT CALLBACK Window::WindowProc(HWND _hwnd, UINT _msg, WPARAM _wp, LPARAM _lp)
{
	// 作成時に渡されたthisポインタをウィンドウに紐づける
	if (_msg == WM_NCCREATE)
	{
		CREATESTRUCT* createData{ reinterpret_cast<CREATESTRUCT*>(_lp) };
		Window* window{ reinterpret_cast<Window*>(createData->lpCreateParams) };
		SetWindowLongPtr(_hwnd, GWLP_USERDATA ,reinterpret_cast<LONG_PTR>(window));
		window->hwnd = _hwnd; // CreateWindowExW完了前から正しいHWNDを保持
	}

	// ウィンドウに紐づけれられたthisポインタを取得する
	Window* windowThisPtr{ reinterpret_cast<Window*>(GetWindowLongPtr(_hwnd, GWLP_USERDATA)) };

	// インスタンスが取得できている場合のみメンバ処理
	if (windowThisPtr)
	{
		// wheelメッセージの処理
		if (_msg == WM_MOUSEWHEEL)
		{
			if (windowThisPtr->onWheel)
			{
				windowThisPtr->onWheel(GET_WHEEL_DELTA_WPARAM(_wp));
			}
			return 0;
		}
	}

	// 終了処理
	if (_msg == WM_DESTROY)
	{
		if (windowThisPtr) windowThisPtr->hwnd = nullptr;
		PostQuitMessage(0); // WM_QUITをメッセージキューに投げる
		return 0;
	}

	return DefWindowProc(_hwnd, _msg, _wp, _lp);
}
