#pragma once
#include "InputName.h"

// keyboardの入力を簡易化するクラス
class KeyboardInput
{
public:
	void Update(); // 入力更新

	bool IsPressed(BYTE _key); // 押している間
	bool IsPushed(BYTE _key); // 押した瞬間
	bool IsReleased(BYTE _key); // 話した瞬間
private:
	BYTE currentKeys[256]{};
	BYTE prevKeys[256]{};
};