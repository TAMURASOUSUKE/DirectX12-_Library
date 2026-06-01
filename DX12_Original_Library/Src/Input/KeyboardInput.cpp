#include "InputConstant.h"
#include "KeyboardInput.h"

// 更新
void KeyboardInput::Update()
{
	memcpy(prevKeys, currentKeys, 256);
	if (!GetKeyboardState(currentKeys))
	{
		OutputDebugStringA("入力更新に失敗しています\n");
	}
}

// 前のフレームも今のフレームも押しているなら
bool KeyboardInput::IsPress(int _key)
{
	if ((currentKeys[_key] & MOST_SIGNIFICANT_BIT) && (prevKeys[_key] & MOST_SIGNIFICANT_BIT))
	{
		return true;
	}
	return false;
}

// 前のフレームでは押されておらず今のフレームで押しているなら
bool KeyboardInput::IsPushed(int _key)
{
	if ((currentKeys[_key] & MOST_SIGNIFICANT_BIT) && !(prevKeys[_key] & MOST_SIGNIFICANT_BIT))
	{
		return true;
	}
	return false;
}

// 前のフレームで押していて今のフレームで押していないなら
bool KeyboardInput::IsReleased(int _key)
{
	if (!(currentKeys[_key] & MOST_SIGNIFICANT_BIT) && (prevKeys[_key] & MOST_SIGNIFICANT_BIT))
	{
		return true;
	}
	return false;
}