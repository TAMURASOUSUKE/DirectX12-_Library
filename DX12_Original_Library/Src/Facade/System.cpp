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
 bool SystemInternal::ProcessMessage()
 {

 }
 void SystemInternal::Finish()
 {

 }
 HWND SystemInternal::GetHWND()
 {

 }
 void SystemInternal::SetOnWheel(std::function<void(short)> _func)
 {

 }
 Vector2Int SystemInternal::GetClientSize()
 {

 }
 Vector2Int System::GetClientSize()
 {

 }
 void System::SetWindowTitle(const wchar_t* _title)
 {

 }
 bool System::IsWindowFocused()
 {

 }
 void System::RequestQuit()
 {

 }


