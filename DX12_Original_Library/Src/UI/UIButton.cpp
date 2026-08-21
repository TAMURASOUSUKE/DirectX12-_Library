#include <utility>
#include "../Debug/DebugLogs.h"
#include "UIButton.h"


UIButton::UIButton(UIButtonInputSource _inputSource)
{
	// 有効値か確認する必要があるのでイニシャライザーではなく代入
	if (_inputSource.IsValid()) inputSource = _inputSource;
	else DEBUG_LOG_ERROR("UIButtonInputSourceに不正な値があります\n");
}

void UIButton::SetInputSource(UIButtonInputSource _inputSource)
{
	if (_inputSource.IsValid()) inputSource = _inputSource;
	else DEBUG_LOG_ERROR("UIButtonInputSourceに不正な値があります\n");
	isArmed = false; // 前のキーの状態を引き継がないようにするためにfalse
}

void UIButton::Update(bool _isTarget)
{
	state = {}; // pushedとreleasedが1フレームだけの状態なのでリセットを掛ける
	if (!isEnabled)
	{
		visualState = UIButtonVisualState::Disabled; // 無効状態にする
		isArmed = false; // 押下開始状態を解除する
		return; // 操作不能状態なら終了
	}

	state.isTarget = _isTarget;
	 
	// このボタンを選択中に押したら記録
	if (state.isTarget && inputSource.IsPushed())
	{
		isArmed = true;
		state.isPushed = true;
	}
	// 押しながら外へ移動しても入力を受け持っているときはheldを維持する
	state.isHeld = isArmed && inputSource.IsHeld();

	// 離した瞬間解除
	if (inputSource.IsReleased())
	{
		state.isReleased = isArmed; // このボタンで始まった入力だけをReleasedとして扱う
		state.isActivated = isArmed && state.isTarget; // 対象中なら操作成立
		state.isCanceled = isArmed && !state.isTarget; // 対象外なら操作中止
		isArmed = false;
	}

	// 表示状態を決める
	if (state.isTarget && isArmed && (state.isPushed || state.isHeld)) visualState = UIButtonVisualState::Pressed;
	else if(state.isTarget)visualState = UIButtonVisualState::Hovered;
	else visualState = UIButtonVisualState::Normal;

	// イベントを呼ぶ
	InvokeEvents();
}

void UIButton::SetOnTarget(EventCallback _callback)
{
	// マウスが乗っている場合だけでなくコントローラーやキーボードで選択されているときも毎フレーム呼ばれる
	onTarget = std::move(_callback);
}

void UIButton::SetOnPushed(EventCallback _callback)
{
	onPushed = std::move(_callback);
}

void UIButton::SetOnHeld(EventCallback _callback)
{
	onHeld = std::move(_callback);
}

void UIButton::SetOnReleased(EventCallback _callback)
{
	onReleased = std::move(_callback);
}

void UIButton::SetOnActivated(EventCallback _callback)
{
	onActivated = std::move(_callback);
}

void UIButton::SetOnCanceled(EventCallback _callback)
{
	onCanceled = std::move(_callback);
}

void UIButton::InvokeEvents()
{
	// 選択中
	if (state.isTarget && onTarget) onTarget();
	// 押された瞬間
	if (state.isPushed && onPushed) onPushed();
	// 押している間
	if (state.isHeld && onHeld) onHeld();
	// 離した瞬間
	if (state.isReleased && onReleased) onReleased();
	// 操作が成立した
	if (state.isActivated && onActivated) onActivated();
	// 操作がキャンセルされた
	else if (state.isCanceled && onCanceled) onCanceled();
}
