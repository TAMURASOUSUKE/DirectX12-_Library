#pragma once
#include "../Math/Vector/Vector2Int.h"

// OSやアプリケーション全体に関する機能を提供する
namespace System
{
	enum class WindowMode
	{
		Windowed, // Windowモード
		BorderlessFullscreen // ボーダーレスフルスクリーン
	};

	// タイトルバーを除いた、実際に描画できる領域を取得する
	Vector2Int GetClientSize();
	// ウィンドウタイトルを変更する
	void SetWindowTitle(const wchar_t* _title);
	// ウィンドウが現在操作対象になっているか
	bool IsWindowFocused();
	// ゲームの終了を要求する
	void RequestQuit();
}
