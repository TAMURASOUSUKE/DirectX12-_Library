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

}

void InputInternal::Fnish()
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

}

bool Input::IsPushed(int _key)
{

}

bool Input::IsReleased(int _key)
{

}

// キーボード限定

bool Input::IsKeyPress(int _key)
{
	keyboard.IsPress(_key);
}

bool Input::IsKeyPushed(int _key)
{
	keyboard.IsPushed(_key);
}

bool Input::IsKeyReleased(int _key)
{
	keyboard.IsReleased(_key);
}

// パッド限定

bool Input::IsPadPress(int _key)
{

}

bool Input::IsPadPushed(int _key)
{

}

bool Input::IsPadReleased(int _key)
{

}