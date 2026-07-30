#pragma once
#include <windows.h>
#include <functional>
#include "System.h"
#include "../Math/Vector/Vector2Int.h"

// Sysytem機能のユーザーに公開しない物
namespace SystemInternal
{
	// Windowを生成する
	bool Initialize(const wchar_t* _title, int _width, int _height, System::WindowMode _mode);
	// Windowを破棄する
	void Finish();
	// GfxとInputへ渡す内部用Windowハンドル
	HWND GetHWND();
	// マウスホイール入力の通知先を登録する
	void SetOnWheel(std::function<void(short)> _func);
	// 実際のクライアント領域を取得する
	Vector2Int GetClientSize();
}
