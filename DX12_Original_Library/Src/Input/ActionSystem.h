#pragma once
#include <vector>
#include "InputName.h"
// 参照引数に完全な型は必要ないので前方宣言で済ませる
class KeyboardInput;
class MouseInput;
class GamePadInput;

// 物理デバイスを抽象化し、入力を扱えるようにするクラス
class ActionSystem
{
public:
	// ユーザーが定義したアクション分のvectorを確保する
	void Setup(int _actionCount); 
	// 該当アクション,設定したいキーで抽象化を行う
	void SetAction(int _action, Binding _binding);
	// 各状態を更新
	void Update(KeyboardInput& _kb, MouseInput& _ms, GamePadInput& _pad);

	bool IsPress(int _action); // 押している間
	bool IsPushed(int _action); // 押した瞬間
	bool IsReleased(int _action); // 離した瞬間

private:
	// アクション一つ分の状態を持つ構造体
	struct  ActionState
	{
		std::vector<Binding> bindings; // 一つのアクションに複数の入力タイプ
		bool  current{ false }; // 現在の状態
		bool prev{ false }; // 1フレーム前の状態
	};

	// サイズチェック用ヘルパー
	bool IsInSizeLimit(int _value) const;

private:

	std::vector<ActionState> actions; // アクションの配列(ユーザーenumの値)

};