#pragma once
#include <windows.h>


// ウィンドウ作成を行うクラス
class Window
{
public:
	void GenerateWindow(); // ウィンドウ作成

	// ウィンドウ名前を設定する関数
	void SetWindowName(const wchar_t* _windowName)
	{
		if (_windowName != nullptr)
		{
			windowName = _windowName;
		}
	}

	HWND GetHWND() const { return hwnd; } // ウィンドウハンドルの取得

private:
	static LRESULT CALLBACK WindowProc(HWND _hwnd, UINT _msg, WPARAM _wp, LPARAM _lp); // カスタムのプロシージャ

 private:
	 HWND hwnd{}; // ウィンドウハンドル
	const wchar_t* windowName{}; // ウィンドウの名前

};