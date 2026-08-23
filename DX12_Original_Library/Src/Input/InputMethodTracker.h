#pragma once
#include "InputName.h"
// 何によって操作しているか追跡するクラス
class InputMethodTracker
{
public:
	InputMethodTracker() = default;
	~InputMethodTracker() = default;

	InputMethodTracker(const InputMethodTracker&) = delete;
	InputMethodTracker& operator=(const InputMethodTracker& _other) = delete;

	// このフレームで各デバイスの操作があったかを受け取りその状況を読み取ってInputMethodを変更する
	void Update(bool _isKeyboardMouse, bool _isGamePad);

	// 今の入力方法を返す
	InputMethod GetCurrentInputMethod() const { return inputMethod; }
private:
	InputMethod inputMethod{ InputMethod::KeyboardMouse }; // 操作状態
};
