#include "../Input/KeyboardInput.h"
#include "../Input/GamePadInput.h"
#include "InputInternal.h"
#include "Input.h"

namespace
{
	KeyboardInput keyboard{}; // キーボード入力クラス
	GamePadInput gamePad{}; // ゲームパッド入力クラス
}

bool InputInternal::Initialize()
{
	return true;
}

void InputInternal::Finish()
{

}

void InputInternal::BeginFrame()
{
	keyboard.Update(); // キーボードの入力更新
	gamePad.Update(); // ゲームパッドの入力更新
}

void InputInternal::EndFrame()
{
	
}


// 抽象化

bool Input::IsPress(int _key)
{
	return false;
}

bool Input::IsPushed(int _key)
{
	return false;
}

bool Input::IsReleased(int _key)
{
	return false;
}

// キーボード限定

bool Input::IsKeyPress(KeyCode::Button _key)
{
	return keyboard.IsPress(static_cast<int>(_key));
}

bool Input::IsKeyPushed(KeyCode::Button _key)
{
	return keyboard.IsPushed(static_cast<int>(_key));
}

bool Input::IsKeyReleased(KeyCode::Button _key)
{
	return keyboard.IsReleased(static_cast<int>(_key));
}

// パッド限定

bool Input::IsPadPress(PadCode::Button _key)
{
	return gamePad.IsPress(static_cast<int>(_key));
}

bool Input::IsPadPress(PadCode::Trigger _trigger)
{
	return gamePad.IsPress(_trigger);
}

bool Input::IsPadPushed(PadCode::Button _key)
{
	return gamePad.IsPushed(static_cast<int>(_key));
}

bool Input::IsPadPushed(PadCode::Trigger _trigger)
{
	return gamePad.IsPushed(_trigger);
}

bool Input::IsPadReleased(PadCode::Button _key)
{
	return gamePad.IsReleased(static_cast<int>(_key));
}

bool Input::IsPadReleased(PadCode::Trigger _trigger)
{
	return gamePad.IsReleased(_trigger);
}

float Input::GetPadTriggerValue(PadCode::Trigger _trigger)
{
	return gamePad.GetTriggerValue(_trigger);
}

Vector2 Input::GetPadStickValue(PadCode::Stick _stick, bool _isInverseY)
{
	return gamePad.GetStickValue(_stick, _isInverseY);
}