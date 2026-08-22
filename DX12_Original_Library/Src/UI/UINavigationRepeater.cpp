#include <cmath>
#include "../Debug/DebugLogs.h"
#include "UINavigationRepeater.h"

bool UINavigationRepeater::Setup(UINavigationRepeatSettings _setting)
{
	if (!_setting.IsValid())
	{
		DEBUG_LOG_ERROR("UINavigationRepeaterに不正な設定値が渡されました\n");
		return false;
	}
	setting = _setting;
	Reset();
	return true;
}

void UINavigationRepeater::Reset()
{
	heldDirection = UINavigationDirection::None; // リセット
	remainingNextRepeatTime = 0.0f;
}

UINavigationDirection UINavigationRepeater::Update(Vector2 _input, float _unscaledDeltaTime)
{
	// _unscaledDeltaTimeが有限値か見る
	if (!std::isfinite(_unscaledDeltaTime) || _unscaledDeltaTime < 0.0f)
	{
		DEBUG_LOG_ERROR("UI選択方向更新に不正なUnscaledDeltaTimeが渡されています\n");
		return UINavigationDirection::None;
	}

	if (!std::isfinite(_input.x) || !std::isfinite(_input.y))
	{
		DEBUG_LOG_ERROR("UINavigationRepeaterに不正な入力値が渡されました\n");
		Reset();
		return UINavigationDirection::None;
	}

	const float inputLengthSquared{ _input.LengthSquared() }; // 長さの二乗を出す

	// 入力がない場合
	if (heldDirection == UINavigationDirection::None)
	{
		// 入力開始時は大きいほうの閾値を超える必要がある
		if (inputLengthSquared < setting.enterThreshold * setting.enterThreshold) return UINavigationDirection::None;
	}
	else
	{
		// 入力開始後は小さい方の閾値まで戻ったら解除
		if (IsNeutral(_input))
		{
			Reset();
			return UINavigationDirection::None;
		}
	}

		// 開始条件を満たしたか入力を継続しているので方向を決める
		const UINavigationDirection inputDirection{ ChooseDirection(_input) };
		// 今まで入力されいた方向と今回入力された方向が違う場合
		if (heldDirection != inputDirection)
		{
			heldDirection = inputDirection; // 今回の方向に更新
			remainingNextRepeatTime = setting.initialDelay; // リピートするまでの時間を設定
			return heldDirection; // はじめの一回は待たずに即返す
		}

		// 同じ入力なら時間を減らす
		remainingNextRepeatTime -= _unscaledDeltaTime;
		// 残り時間がまだあるときはNoneを返す
		if (remainingNextRepeatTime > 0.0f) return UINavigationDirection::None;

		// 次に同じ方向へ入力されていた時のためにリピート用の時間を入れておく
		remainingNextRepeatTime = setting.repeatInterval;
		return heldDirection;
}


UINavigationDirection UINavigationRepeater::ChooseDirection(Vector2 _input) const
{
	// X軸とY軸の入力の大きさを見て大きいほうの軸を最終的な方向に決定づけることで斜め入力で2回移動が起こることを避ける
	if (std::abs(_input.x) > std::abs(_input.y)) return _input.x >= 0.0f ? UINavigationDirection::Right : UINavigationDirection::Left;
	else return _input.y >= 0.0f ? UINavigationDirection::Down : UINavigationDirection::Up;
}

bool UINavigationRepeater::IsNeutral(Vector2 _input) const
{
	const float stickLengthSquared{ _input.LengthSquared() }; // 長さの二乗を出す
	return stickLengthSquared < setting.releaseThreshold * setting.releaseThreshold; // 入力値がreleaseの閾値より小さいなら中央に戻ったと判定する
}
