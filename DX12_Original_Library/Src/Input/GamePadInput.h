#pragma once
#include <XInput.h>
#include "InputBase.h"
#pragma comment(lib, "Xinput.lib")

// コントローラーからの入力を受け取る(現在はXboxのみで今後Switch,PSへ拡張)
class GamePadInput : public InputBase
{
public:
	void Update() override; // 入力更新

	bool IsPress(int _key) override; // 押している間
	bool IsPushed(int _key) override; // 押した瞬間
	bool IsReleased(int _key) override; // 離した瞬間

private:
	XINPUT_GAMEPAD currentPad{}; // 現在の入力
	XINPUT_GAMEPAD prevPad{}; // 1フレーム前の入力
};
