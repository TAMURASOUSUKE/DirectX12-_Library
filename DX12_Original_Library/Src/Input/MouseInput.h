#pragma once
#include <windows.h>
#include "InputBase.h"

// マウスの入力を簡易化するクラス
class MouseInput : public InputBase
{
public:
	void Update() override; // 入力更新

	bool IsPress(int _click) override; // 押している間
	bool IsPushed(int _click) override; // 押した瞬間
	bool IsReleased(int _click) override; // 離した瞬間
private:
	BYTE currentClicks[256]{};
	BYTE prevClicks[256]{};
};