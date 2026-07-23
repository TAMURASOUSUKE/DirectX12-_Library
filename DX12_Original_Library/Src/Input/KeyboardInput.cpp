#include "InputConstant.h"
#include "KeyboardInput.h"

// 更新
void KeyboardInput::Update()
{
	memcpy(prevKeys, currentKeys, 256);
	BOOL result{ GetKeyboardState(currentKeys) };
	if (!result) memset(currentKeys, 0, 256); // 0リセットで入力を残さない
}

// 今のフレームで押しているなら
bool KeyboardInput::IsPress(int _key)
{
	return(currentKeys[_key] & MOST_SIGNIFICANT_BIT);
}

// 前のフレームでは押されておらず今のフレームで押しているなら
bool KeyboardInput::IsPushed(int _key)
{
	return (currentKeys[_key] & MOST_SIGNIFICANT_BIT) && !(prevKeys[_key] & MOST_SIGNIFICANT_BIT);
}

// 前のフレームで押していて今のフレームで押していないなら
bool KeyboardInput::IsReleased(int _key)
{
	 return!(currentKeys[_key] & MOST_SIGNIFICANT_BIT) && (prevKeys[_key] & MOST_SIGNIFICANT_BIT);
}