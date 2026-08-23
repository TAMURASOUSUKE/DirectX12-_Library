#pragma once
#include "../Math/Vector/Vector2.h"
#include "UINavigationType.h"

// 入力ベクトルと時間から移動すべき一方向を返すクラス
class UINavigationRepeater
{
public:
	UINavigationRepeater() = default;
	~UINavigationRepeater() = default;

	UINavigationRepeater(const UINavigationRepeater& _other) = delete;
	UINavigationRepeater& operator=(const UINavigationRepeater& _other) = delete;

	// 初期化関数
	bool Setup(UINavigationRepeatSettings _setting);

	// 状態をリセットする
	void Reset();

	// 入力方向と時間を受け取り更新を行い1方向を返す
	UINavigationDirection Update(Vector2 _input, float unscaledDeltaTime);

private:
	// ベクトルから上下左右を一つ選ぶ
	UINavigationDirection ChooseDirection(Vector2 _input) const;

	// 中立へ戻ったか判定する
	bool IsNeutral(Vector2 _input) const;

private:
	UINavigationRepeatSettings setting{}; // リピート設定
	UINavigationDirection heldDirection{ UINavigationDirection::None }; // 入力し続けている方向
	float remainingNextRepeatTime{ 0.0f }; // 次のリピートまでの残り時間

};
