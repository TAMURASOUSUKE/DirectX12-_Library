#include "../Debug/DebugLogs.h"
#include "Window.h"

namespace {
	// OSへ登録するWindowClassの識別名
	// 表示タイトルとは別物なので固定
	constexpr wchar_t WINDOW_CLASS_NAME[]{ L"TSGameLibWindowClass" };
}

namespace {
	// WindowSytleの変更を行う
	bool TrySetWindowStyle(HWND _hwnd, LONG_PTR _style)
	{
		SetLastError(ERROR_SUCCESS);
		const LONG_PTR prevStyle{ SetWindowLongPtrW(_hwnd, GWL_STYLE, _style) };

		// 戻り値	0は正常の場合もあるためGetLastErrorも確認する
		if (prevStyle == 0 && GetLastError() != ERROR_SUCCESS)
		{
			DEBUG_LOG_ERROR("WindowStyleの変更に失敗しました ErrorCode : {}\n", GetLastError());
			return false;
		}
		return true;
	}

	// カーソルの表示状態を設定する補助関数
	void ForceCursorVisible(bool _visible)
	{
		// ShowCursorはbool設定ではなく内部カウンター方式なので複数回呼ばれても任意の設定にできるように指定した状態になるまでwhileを回す
		if (_visible) while (ShowCursor(true) < 0); // trueの場合(表示)はカウンターがインクリメントされるので正になるまで
		else while (ShowCursor(false) >= 0); // falseの場合(非表示)はカウンターがデクリメントされるので負になるまで
	}
}

bool Window::GenerateWindow(int _clientWidth, int _clientHeight)
{
	if (_clientWidth <= 0 || _clientHeight <= 0)
	{
		DEBUG_LOG_ERROR("ウィンドウのサイズには0より大きい値を渡してください\n");
		return false;
	}
	// 情報の保存
	preferredWindowedClientSize = { _clientWidth, _clientHeight };

	if (hwnd != nullptr)
	{
		DEBUG_LOG_ERROR("ウィンドウはすでに生成されています\n");
		return false;
	}

	// ドラッグリサイズと最大化を許可しない
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

	if (!GenerateNativeWindow(WINDOW_STYLE, CW_USEDEFAULT, CW_USEDEFAULT, windowWidth, windowHeight)) return false;

	// 成功後に状態を設定
	isBorderlessFullscreen = false;
	hasWindowedPlacement = false;
	return true;
}
bool Window::GenerateBorderlessFullscreen(int _windowedClientWidth, int _windowedClientHeight)
{
	// 情報を保存
	if (_windowedClientWidth <= 0 || _windowedClientHeight <= 0)
	{
		DEBUG_LOG_ERROR("Window復元用サイズが不正です\n");
		return false;
	}
	preferredWindowedClientSize = { _windowedClientWidth, _windowedClientHeight };

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

	if (!GenerateNativeWindow(BORDERLESS_STYLE, monitorRect.left, monitorRect.top, monitorWidth, monitorHeight)) return false;

	// 成功後に状態を設定
	isBorderlessFullscreen = true;
	hasWindowedPlacement = false;
	return true;
}

void Window::Shutdown()
{
	ClipCursor(nullptr);
	ForceCursorVisible(true);
	// Inputへのコールバック参照を先に切る
	onWheel = {};
	onResize = {};
	onCursorWarp = {};
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

bool Window::SetCursorState(bool _visible, bool _locked)
{
	if (!hwnd) return false;
	if (_locked) _visible = false; // 固定されている時は必ず非表示

	// 失敗した時用に保存
	const bool prevVisible{ cursorVisible };
	const bool prevLocked{ cursorLocked };

	// 状態の適用
	cursorVisible = _visible;
	cursorLocked = _locked;

	// 状態の確定
	if (!ApplyCursorState(IsFocused()))
	{
		// もとの状態に戻す
		cursorVisible = prevVisible;
		cursorLocked = prevLocked;
		ApplyCursorState(IsFocused()); //前と同じ状態なので成功する前提
		return false;
	}
	// 固定の場合は中央にする
	if (cursorLocked && IsFocused()) CenterCursor();
	return true;
}

void Window::UpdateCursorLock()
{
	// 固定の場合は中央にし続ける
	if (cursorLocked && IsFocused()) CenterCursor();
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

bool Window::SetBorderlessFullscreen(bool _enabled)
{
	if (!hwnd || !IsWindow(hwnd))
	{
		DEBUG_LOG_ERROR("ウィンドウが生成されていないためモードを変更できません\n");
		return false;
	}
	// すでに目的のモードなら正常終了
	if (_enabled == isBorderlessFullscreen) return true;

	// Windowed->BorderlessFullscreenの場合
	if (_enabled)
	{
		WINDOWPLACEMENT temporaryPlacement{};
		temporaryPlacement.length = sizeof(WINDOWPLACEMENT);

		if (!GetWindowPlacement(hwnd, &temporaryPlacement))
		{
			DEBUG_LOG_ERROR("Windowed時の位置とサイズを取得できませんでした\n");
			return false;
		}
		SetLastError(ERROR_SUCCESS);

		const LONG_PTR temporaryWindowedStyle{ GetWindowLongPtrW(hwnd, GWL_STYLE) };
		if (temporaryWindowedStyle == 0 && GetLastError() != ERROR_SUCCESS)
		{
			DEBUG_LOG_ERROR("Windowed時のStyleを取得できませんでした\n");
			return false;
		}

		// 現在Windowが存在するモニターを対象にする
		const HMONITOR monitor{ MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST) };
		if (!monitor)
		{
			DEBUG_LOG_ERROR("Windowが存在するモニターを取得できませんでした\n");
			return false;
		}

		MONITORINFO monitorInfo{};
		monitorInfo.cbSize = sizeof(MONITORINFO);
		if (!GetMonitorInfoW(monitor, &monitorInfo))
		{
			DEBUG_LOG_ERROR("モニター情報を取得できませんでした\n");
			return false;
		}

		// WS_VISIBLE等はお残してWindowed特有の枠だけ外す
		const LONG_PTR borderlessStyle{ (temporaryWindowedStyle & ~static_cast<LONG_PTR>(WS_OVERLAPPEDWINDOW)) | static_cast<LONG_PTR>(WS_POPUP) };
		if (!TrySetWindowStyle(hwnd, borderlessStyle)) return false; // ログは関数内で出す

		const RECT& monitorRect{ monitorInfo.rcMonitor };
		const int monitorWidth{ monitorRect.right - monitorRect.left };
		const int monitorHeight{ monitorRect.bottom - monitorRect.top };

		if (!SetWindowPos(hwnd, HWND_TOP, monitorRect.left, monitorRect.top, monitorWidth, monitorHeight, SWP_NOOWNERZORDER | SWP_FRAMECHANGED))
		{
			DEBUG_LOG_ERROR("BorderlessFullscreenへの変更に失敗しました\n");
			// Styleだけ変更された中途半端な状態を戻す
			TrySetWindowStyle(hwnd, temporaryWindowedStyle);
			return false;
		}
		// 全成功後に保存状態を確定する
		windowedPlacement = temporaryPlacement;
		windowedStyle = temporaryWindowedStyle;
		hasWindowedPlacement = true;
		isBorderlessFullscreen = true;
		return true;
	}

	// BorderlessFullscreen->Windowedにする
	WINDOWPLACEMENT targetPlacement{};
	if (hasWindowedPlacement)
	{
		// Windowedから切り替えた場合はもとの位置へ戻す
		targetPlacement = windowedPlacement;
	}
	else
	{
		// 最初からborderlessだった場合は基準サイズから作る
		if (preferredWindowedClientSize.x <= 0 || preferredWindowedClientSize.y <= 0)
		{
			DEBUG_LOG_ERROR("Windowed復元用サイズが保存されていません\n");
			return false;
		}
		RECT windowRect{ 0,0, preferredWindowedClientSize.x, preferredWindowedClientSize.y };
		const DWORD targetStyle{ static_cast<DWORD>(windowedStyle) };
		if (!AdjustWindowRectEx(&windowRect, targetStyle, false, 0))
		{
			DEBUG_LOG_ERROR("Windowed復元サイズの計算に失敗しました\n");
			return false;
		}
		const int windowWidth{ windowRect.right - windowRect.left };
		const int windowHeight{ windowRect.bottom - windowRect.top };
		const HMONITOR monitor{ MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST) };
		MONITORINFO monitorInfo{};
		monitorInfo.cbSize = sizeof(MONITORINFO);
		if (!monitor || !GetMonitorInfoW(monitor, &monitorInfo))
		{
			DEBUG_LOG_ERROR("Windowed復元先のモニター情報を取得できませんでした\n");
			return false;
		}
		const RECT& workRect{ monitorInfo.rcWork };
		// モニターの作業領域中央へ配置
		const int x{ workRect.left + ((workRect.right - workRect.left) - windowWidth) / 2 };
		const int y{ workRect.top + ((workRect.bottom - workRect.top) - windowHeight) / 2 };

		targetPlacement.length = sizeof(WINDOWPLACEMENT);
		targetPlacement.showCmd = SW_SHOWNORMAL;
		targetPlacement.rcNormalPosition = { x, y , x + windowWidth, y + windowHeight };
	}

	// 初回Borderless軌道では保存StyleにWS_VISIBLEがないため補う
	LONG_PTR targetWindowedStyle{ windowedStyle };
	if (IsWindowVisible(hwnd)) targetWindowedStyle |= WS_VISIBLE;

	// スタイルの設定
	if (!TrySetWindowStyle(hwnd, targetWindowedStyle)) return false;

	// 位置とサイズの復元
	if (!SetWindowPlacement(hwnd, &targetPlacement))
	{
		DEBUG_LOG_ERROR("Windowed時の位置とサイズを復元できませんでした\n");
		return false;
	}

	// 非クライアント領域を再計算
	if (!SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED))
	{
		DEBUG_LOG_ERROR("Windowed時のフレーム再計算に失敗しました\n");
		return false;
	}
	isBorderlessFullscreen = false;
	return true;
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
		this // ウィンドウの機能を自分で設定するためにプロシージャに自身のポインタを渡す
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
		SetWindowLongPtr(_hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
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
		// ウィンドウサイズ変更処理
		if (_msg == WM_SIZE)
		{
			// 最小化中はクライアント領域が0x0になるためGPUリソースを作り直さない
			if (_wp == SIZE_MINIMIZED) return 0;

			const int width{ static_cast<int>(LOWORD(_lp)) };
			const int height{ static_cast<int>(HIWORD(_lp)) };

			if (width > 0 && height > 0 && windowThisPtr->onResize) windowThisPtr->onResize(width, height);
			return 0;
		}
		// マウスカーソル表示対応処理
		if (_msg == WM_SETFOCUS)
		{
			windowThisPtr->ApplyCursorState(true);
			if (windowThisPtr->cursorLocked) windowThisPtr->CenterCursor(); // ロック状態なら中央固定
		}
		// フォーカス解除処理
		if (_msg == WM_KILLFOCUS) windowThisPtr->ApplyCursorState(false);
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

bool Window::ApplyCursorState(bool _isFocused)
{
	// フォーカスを失った場合はゲーム外へカーソルを返す
	if (!_isFocused)
	{
		ClipCursor(nullptr); // 画面全体へ
		ForceCursorVisible(true); // 表示
		return true;
	}
	if (!cursorLocked)
	{
		// NormalとHiddenでは閉じ込めない
		ClipCursor(nullptr); // 画面全体へ
		ForceCursorVisible(cursorVisible);
		return true;
	}

	// Lockedではクライアント領域内に閉じ込める
	RECT clientRect{};
	if (!GetClientRect(hwnd, &clientRect))
	{
		DEBUG_LOG_ERROR("カーソル固定用ClientRectを取得できませんでした\n");
		return false;
	}

	POINT leftTop{ clientRect.left, clientRect.top };
	POINT rightBottom{ clientRect.right, clientRect.bottom };
	if (!ClientToScreen(hwnd, &leftTop) || !ClientToScreen(hwnd, &rightBottom))
	{
		DEBUG_LOG_ERROR("カーソル固定領域の座標変換に失敗しました\n");
		return false;
	}

	const RECT clipRect{ leftTop.x, leftTop.y, rightBottom.x, rightBottom.y };
	if (!ClipCursor(&clipRect))
	{
		DEBUG_LOG_ERROR("カーソル固定領域を設定できませんでした\n");
		return false;
	}
	ForceCursorVisible(false);
	return true;
}

bool Window::CenterCursor()
{
	if (!hwnd || !cursorLocked || !IsFocused()) return false;

	RECT clientRect{};
	if (!GetClientRect(hwnd, &clientRect))
	{
		DEBUG_LOG_ERROR("クライアント領域の取得に失敗しました\n");
		return false;
	}
	POINT center{ (clientRect.right - clientRect.left) / 2, (clientRect.bottom - clientRect.top) / 2 };
	if (!ClientToScreen(hwnd, &center))
	{
		DEBUG_LOG_ERROR("カーソル固定領域の座標変換に失敗しました\n");
		return false;
	}
	if (!SetCursorPos(center.x, center.y))
	{
		DEBUG_LOG_ERROR("カーソルを中央へ移動できませんでした\n");
		return false;
	}

	// Input側に追跡意図を合わせてもらう
	if (onCursorWarp) onCursorWarp();
	return true;
}
