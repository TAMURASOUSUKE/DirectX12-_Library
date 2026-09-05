#include <utility>
#include "../Window/Window.h"
#include "SystemInternal.h"
#include "../Debug/DebugLogs.h"
#include "System.h"

namespace
{
	Window window{}; // ここを唯一の所有者とする
}

bool SystemInternal::Initialize(const wchar_t* _title, int _width, int _height, System::WindowMode _mode)
{
	if (_width <= 0 || _height <= 0) return false;
	 // 生成前にタイトルを保存
	 if (!window.SetWindowTitle(_title)) return false;

	 bool generated{ false };
	 // モードによって分岐する
	 switch (_mode)
	 {
	 case System::WindowMode::Windowed:
		 generated = window.GenerateWindow(_width, _height);
		 break;
	 case System::WindowMode::BorderlessFullscreen:
		 generated = window.GenerateBorderlessFullscreen(_width, _height);
		 break;
	 default:
		 DEBUG_LOG_ERROR("不明なWindowModeが指定されました\n");
		 return false;
	 }

	 if (!generated)
	 {
		 // ウィンドウクラスの登録後に失敗している可能性を考慮してシャットダウンする
		 window.Shutdown();
		 return false;
	 }
	 return true;
}

 void SystemInternal::Finish()
 {
	 window.Shutdown();
 }

 void SystemInternal::UpdateCursorLock()
 {
	 window.UpdateCursorLock();
 }

 void SystemInternal::SetOnWheel(std::function<void(short)> _func)
 {
	 window.SetOnWheel(std::move(_func));
 }

 void SystemInternal::SetOnResize(std::function<void(int, int)> _func)
 {
	 window.SetOnResize(std::move(_func));
 }

 void SystemInternal::SetOnCursorWarp(std::function<void()> _func)
 {
	 window.SetOnCursorWarp(std::move(_func));
 }

 HWND SystemInternal::GetHWND()
 {
	 return window.GetHWND();
 }

 Vector2Int SystemInternal::GetClientSize()
 {
	 return window.GetClientSize();
 }

 Vector2Int System::GetClientSize()
 {
	 return window.GetClientSize();
 }

 bool System::SetCursorMode(CursorMode _mode)
 {
	 switch (_mode)
	 {
	 case System::CursorMode::Normal:
		 return window.SetCursorState(true, false);
	 case System::CursorMode::Hidden:
		 return window.SetCursorState(false, false);
	 case System::CursorMode::Locked:
		 return window.SetCursorState(false, true);
	 default:
		 DEBUG_LOG_ERROR("不明なCursorModeが渡されました\n");
		 return false;
	 }
 }

 System::CursorMode System::GetCursorMode()
 {
	 if (window.IsCursorLocked()) return CursorMode::Locked;
	 if (!window.IsCursorVisible()) return CursorMode::Hidden;
	 return CursorMode::Normal;
 }

 void System::SetWindowTitle(const wchar_t* _title)
 {
	 window.SetWindowTitle(_title);
 }

 bool System::IsWindowFocused()
 {
	 return window.IsFocused();
 }

 bool System::SetWindowMode(WindowMode _mode)
 {
	 switch (_mode)
	 {
	 case System::WindowMode::Windowed:
		 return window.SetBorderlessFullscreen(false);
	 case System::WindowMode::BorderlessFullscreen:
		 return window.SetBorderlessFullscreen(true);
	 default:
		 DEBUG_LOG_ERROR("不明なWindowModeが指定されました\n");
		 return false;
	 }
 }

 System::WindowMode System::GetWindowMode()
 {
	 if (window.IsBorderlessFullscreen()) return WindowMode::BorderlessFullscreen;
	 return WindowMode::Windowed;
 }

 bool System::SetWindowIcon(int _resourceID)
 {
	 return window.SetWindowIcon(_resourceID);
 }

 void System::RequestQuit()
 {
	 window.RequestQuit();
 }


