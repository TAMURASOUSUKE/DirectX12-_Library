#include "../Input/InputConstant.h"
#include "../Input/KeyboardInput.h"
#include "../Input/InputName.h"
#include "InputInternal.h"
#include "Input.h"

namespace
{
	KeyboardInput keyboard{}; // キーボード入力クラス
}

bool InputInternal::Initialize()
{
	return false;
}

void InputInternal::Finish()
{

}

void InputInternal::BeginFrame()
{
	keyboard.Update(); // キーボードの入力更新
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

bool Input::IsKeyPress(int _key)
{
	return keyboard.IsPress(_key);
}

bool Input::IsKeyPushed(int _key)
{
	return keyboard.IsPushed(_key);
}

bool Input::IsKeyReleased(int _key)
{
	return keyboard.IsReleased(_key);
}

// パッド限定

bool Input::IsPadPress(int _key)
{
	return false;
}

bool Input::IsPadPushed(int _key)
{
	return false;
}

bool Input::IsPadReleased(int _key)
{
	return false;
}