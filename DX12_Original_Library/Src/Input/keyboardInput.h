#pragma once
#include "InputBase.h"
#include "InputName.h"

// keyboardの入力を簡易化するクラス
class KeyboardInput : public InputBase
{
public:
	void Update() override; // 入力更新

	bool IsPress(int _key) override; // 押している間
	bool IsPushed(int _key) override; // 押した瞬間
	bool IsReleased(int _key) override; // 話した瞬間
private:
	BYTE currentKeys[256]{};
	BYTE prevKeys[256]{};
};