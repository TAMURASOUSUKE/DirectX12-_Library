#include"../Debug/DebugLogs.h"
#include "../Input/KeyboardInput.h"
#include "../Input/MouseInput.h"
#include "../Input/GamePadInput.h"
#include "../Input/ActionSystem.h"
#include "InputInternal.h"
#include "Input.h"

namespace
{
	KeyboardInput keyboard{}; // キーボード入力クラス
	MouseInput mouse{}; // マウス入力クラス
	GamePadInput gamePad{}; // ゲームパッド入力クラス
	ActionSystem actionSystem{}; // 抽象化入力クラス
}

bool InputInternal::Initialize(HWND _hwnd)
{
	DEBUG_ASSERT(_hwnd != nullptr && "InputInternalでnull状態のHWNDが渡されました\n");
	if (_hwnd != nullptr)
	{
		mouse.Initialize(_hwnd);
		return true;
	}
	return false;
}

void InputInternal::Finish()
{

}

void InputInternal::BeginFrame()
{
	keyboard.Update(); // キーボードの入力更新
	mouse.Update(); // マウスの更新
	gamePad.Update(); // ゲームパッドの入力更新
	actionSystem.Update(keyboard, mouse, gamePad); // 抽象化の入力更新
}

void InputInternal::EndFrame()
{
	
}


// 抽象化
void Input::Detail::SetupActionImpl(int _count)
{
	actionSystem.Setup(_count);
}

void Input::Detail::SetActionImpl(int _action, Binding _binding)
{
	actionSystem.SetAction(_action, _binding);
}

bool Input::Detail::IsActionPressImpl(int _action)
{
	return actionSystem.IsPress(_action);
}

bool Input::Detail::IsActionPushedImpl(int _action)
{
	return actionSystem.IsPushed(_action);
}

bool Input::Detail::IsActionReleasedImpl(int _action)
{
	return actionSystem.IsReleased(_action);
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

// マウス限定

bool Input::IsMousePress(MouseCode::Click _click)
{
	return mouse.IsPress(static_cast<int>(_click));
}

bool Input::IsMousePushed(MouseCode::Click _click)
{
	return mouse.IsPushed(static_cast<int>(_click));
}

bool Input::IsMouseReleased(MouseCode::Click _click)
{
	return mouse.IsReleased(static_cast<int>(_click));
}

int Input::GetMouseWheelValue()
{
	return mouse.GetWheelValue();
}

int Input::GetMouseWheelNotchValue()
{
	return mouse.GetWheelNotchValue();
}

Vector2Int Input::GetMousePoint()
{
	return mouse.GetCursorPoint();
}

Vector2Int Input::GetMouseDelta()
{
	return mouse.GetCursorDelta();
}

void InputInternal::AddMouseWheelDelta(short _delta)
{
	mouse.AddWheelDelta(_delta);
}

bool InputInternal::ResetMouseCursorTracking()
{
	return mouse.ResetCursorTracking();
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
