#include "InputMethodTracker.h"

void InputMethodTracker::Update(bool _isKeyboardMouse, bool _isGamePad)
{
	// 各状態を見て入力方法を決定する 両方入力がある状態または両方ともない状態なら前回を維持する
	if (_isKeyboardMouse && !_isGamePad) inputMethod = InputMethod::KeyboardMouse;
	else if (!_isKeyboardMouse && _isGamePad) inputMethod = InputMethod::GamePad;
}
