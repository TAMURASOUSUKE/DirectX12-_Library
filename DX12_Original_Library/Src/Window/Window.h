#pragma once
#include <windows.h>
#include <functional>


// ウィンドウ作成を行うクラス
class Window
{
public:
	bool GenerateWindow(int _clientWidth, int _clientHeight); // ウィンドウ作成

	// ウィンドウ名前を設定する関数
	void SetWindowName(const wchar_t* _windowName)
	{
		if (_windowName != nullptr)
		{
			windowName = _windowName;
		}
	}

	// オブザーバーパターンの監視者される側として値を伝えるためのコールバック
	void SetOnWheel(std::function<void(short)> _func)
	{
		if (_func != nullptr)
		{
			onWheel = _func;
		}
	}

	HWND GetHWND() const { return hwnd; } // ウィンドウハンドルの取得

private:
	static LRESULT CALLBACK WindowProc(HWND _hwnd, UINT _msg, WPARAM _wp, LPARAM _lp); // カスタムのプロシージャ

 private:
	 std::function<void(short)> onWheel{}; // 回転量計算用
	 HWND hwnd{}; // ウィンドウハンドル
	 const wchar_t* windowName{ L"DefaultWindow" }; // ウィンドウの名前

};
