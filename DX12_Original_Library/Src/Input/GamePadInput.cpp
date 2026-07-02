#include <windows.h>
#include "GamePadInput.h"

void GamePadInput::Update()
{
	prevPad = currentPad; // 状態保存
	XINPUT_STATE state{};
	DWORD result{ XInputGetState(0, &state) };
	if (ERROR_SUCCESS == result)
	{
		currentPad = state.Gamepad;
	}
	else
	{
		currentPad = {}; // 入力は残さない
	}
}

// 押されている間(1フレーム目からみる)
bool GamePadInput::IsPress(int _key)
{
	return (currentPad.wButtons & _key);
}

// 押した瞬間
bool GamePadInput::IsPushed(int _key)
{
	return (currentPad.wButtons & _key) && !(prevPad.wButtons & _key);
}

// 離した瞬間
bool GamePadInput::IsReleased(int _key)
{
	return !(currentPad.wButtons & _key) && (prevPad.wButtons & _key);
}