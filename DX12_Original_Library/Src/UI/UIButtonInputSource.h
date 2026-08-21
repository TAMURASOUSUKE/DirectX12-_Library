#pragma once
#include <functional>

// 入力方法を受け取り、入力状態を調べboolに変換するクラス
class UIButtonInputSource
{
public:
	// boolを返す関数を受け取る
	using InputQuery = std::function<bool()>;
	UIButtonInputSource() = default;
	// 押した瞬間、押している間、離した瞬間の処理を受け取る
	UIButtonInputSource(InputQuery _pushed, InputQuery _heldQuery, InputQuery _releasedQuery);

	// 各入力状態を問い合わせる
	bool IsPushed() const;
	bool IsHeld() const;
	bool IsReleased() const;

	// 必要な問い合わせ処理がすべて登録されているか
	bool IsValid() const;
private:
	// 各状態の処理
	InputQuery pushedQuery{};
	InputQuery heldQuery{};
	InputQuery releasedQuery{};
};
