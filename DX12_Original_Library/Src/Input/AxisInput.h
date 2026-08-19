#pragma once
#include <vector>
#include <cstddef>
#include "../Math/Vector/Vector2.h"
#include "InputName.h"
#include "AxisType.h"
// 参照引数に完全な型は必要ないので前方宣言で済ませる
class KeyboardInput;
class MouseInput;
class GamePadInput;

// 軸入力を使うものを抽象化して提供するクラス
class AxisInput
{
public:
	// ユーザーが定義したアクション分vectorを確保する
	void SetupAxisCount(int  _count);

	// AxisがValue型かDelta型かを設定する
	void SetAxisMode(int  _axis, AxisMode _mode);

	// 一つのAxisに対して物理入力との対応を設定する
	bool AddAxisBinding(int _axis, AxisBinding _binding);
	
	// 各物理入力を読み全Axisの現在値を計算する Delta型ではStickなどの継続入力を1フレーム分にするのでUnscaledDeltaTimeを受け取る
	void Update(KeyboardInput& _keyboard, MouseInput& _mouse, GamePadInput& _pad, InputMethod _method, float _unscaledDeltaTime);

	// 計算済みのAxis値を取得する
	Vector2 GetValue(int  _axis) const;

private:
	// 複数のBinding、計算済みの現在値、Axisの計算方式を持つ
	struct AxisState
	{
		std::vector<AxisBinding> bindings{}; // 複数のBinding
		Vector2 calculatedValue{ Vector2::Zero }; // 計算済みの値
		AxisMode calculationMode{ AxisMode::Value };
	};

	// 指定されたAxis番号が台帳内か確認する
	bool IsInSizeLimit(std::size_t _axis) const;

private:
	std::vector<AxisState> axes{}; // 設定された抽象化入力をまとめる

};
