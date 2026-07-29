#include "../Window/Window.h"
#include "SystemInternal.h"
#include "System.h"

namespace
{
	Window window{}; // ここを唯一の所有者とする
}

 bool SystemInternal::Initialize(const wchar_t* _title, int _width, int _height)
{
	 // 生成前にタイトルを保存
	 if (!window.SetWindowTitle(_title)) return false;
	 // 指定されたクライアントサイズでWindowを生成する
	 if (!window.GenerateWindow(_width, _height))
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
 HWND SystemInternal::GetHWND()
 {
	 return window.GetHWND();
 }
 void SystemInternal::SetOnWheel(std::function<void(short)> _func)
 {
	 window.SetOnWheel(_func);
 }
 Vector2Int SystemInternal::GetClientSize()
 {
	 return window.GetClientSize();
 }
 Vector2Int System::GetClientSize()
 {
	 return window.GetClientSize();
 }
 void System::SetWindowTitle(const wchar_t* _title)
 {
	 window.SetWindowTitle(_title);
 }
 bool System::IsWindowFocused()
 {
	 return window.IsFocused();
 }
 void System::RequestQuit()
 {
	 window.RequestQuit();
 }


