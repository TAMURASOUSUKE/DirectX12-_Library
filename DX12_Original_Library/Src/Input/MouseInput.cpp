#include "InputConstant.h"
#include "MouseInput.h"

void MouseInput::Update()
{
	memcpy(prevClicks, currentClicks, 256);
	BOOL result{ GetKeyboardState(currentClicks) };
	if (!result) memset(currentClicks, 0, 256); // 0リセットで入力を残さない
}

bool MouseInput::IsPress(int _click)
{
	return(currentClicks[_click] & MOST_SIGNIFICANT_BIT);
}

bool MouseInput::IsPushed(int _click)
{
	return (currentClicks[_click] & MOST_SIGNIFICANT_BIT) && !(prevClicks[_click] & MOST_SIGNIFICANT_BIT);
}

bool MouseInput::IsReleased(int _click)
{
	return!(currentClicks[_click] & MOST_SIGNIFICANT_BIT) && (prevClicks[_click] & MOST_SIGNIFICANT_BIT);
}