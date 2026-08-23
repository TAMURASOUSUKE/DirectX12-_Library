#pragma once
#include <functional>
#include "UIButtonType.h"
#include "UIButtonInputSource.h"

// 指定された操作でアクションを起こすことができるボタン
class UIButton
{
public:
	// 引数なし・戻り値なしのイベント関数
	using EventCallback = std::function<void()>;

	UIButton() = default;
	// UIButtonInputSourceがUIButtonに変換されないようにexplicit 
	explicit UIButton(UIButtonInputSource _inputSource); // 入力元を受け取る

	// 入力元を後から変更する
	void SetInputSource(UIButtonInputSource _inputSource);

	// 現在操作対象か受け取り状態を更新する
	void Update(bool _isTarget);

	// 各イベントを登録する
	void SetOnTarget(EventCallback _callback);
	void SetOnPushed(EventCallback _callback);
	void SetOnHeld(EventCallback _callback);
	void SetOnReleased(EventCallback _callback);
	void SetOnActivated(EventCallback _callback);
	void SetOnCanceled(EventCallback _callback);

	// 計算済みの状態を取得する
	const UIButtonState& GetState() const { return state; }

	// 描画に使用する表示状態を取得する
	UIButtonVisualState GetVisualState() const { return visualState; }

	// 操作可能、不可能状態を切り替える
	void SetEnabled(bool _isEnabled) { isEnabled = _isEnabled; }

private:
	// 計算済みのUIButtonStateに対応するイベントを呼ぶ
	void InvokeEvents();

private:
	UIButtonInputSource inputSource; // 入力元
	UIButtonState state{}; // ボタンの状態
	UIButtonVisualState visualState{ UIButtonVisualState::Normal }; // ボタンの表示状態
	bool isEnabled{ true }; // 操作可能か
	bool isArmed{ false }; // このボタン上で押下を開始したか

	// 各登録イベント
	EventCallback onTarget{};
	EventCallback onPushed{};
	EventCallback onHeld{};
	EventCallback onReleased{};
	EventCallback onActivated{};
	EventCallback onCanceled{};
};
