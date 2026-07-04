#pragma once
#include <windows.h>
#include "../Math/TSMath.h"
#include "InputBase.h"

// マウスの入力を簡易化するクラス
class MouseInput : public InputBase
{
public:
	void Initialize(HWND _hwnd);  // 初期化

	void Update() override; // 入力更新

	bool IsPress(int _click) override; // 押している間
	bool IsPushed(int _click) override; // 押した瞬間
	bool IsReleased(int _click) override; // 離した瞬間

	// 左上原点のy軸下向きのクライアント座標を返す。(単位ピクセル)
	Vector2Int GetCursorPoint();
private:
	HWND hwnd{}; // カーソル用ウィンドウハンドル
	BYTE currentClicks[256]{};
	BYTE prevClicks[256]{};
	POINT currentClientCursorPos{}; // 現在のマウスカーソル位置
	POINT prevClientCursorPos{}; // 前フレームのマウスカーソル位置
};