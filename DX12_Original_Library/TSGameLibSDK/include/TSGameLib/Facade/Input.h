#pragma once
#include  <type_traits>
#include "../Math/TSMath.h"
#include "../Input/InputName.h"

// 入力に関する機能をユーザーに提供する
namespace Input
{

	// ユーザーが触ってはならない部分を名前で知らせる
	namespace Detail
	{
		void SetupActionImpl(int _count); // アクション数だけ内部配列を確保する
		void SetActionImpl(int _action, Binding _binding); // アクションと設定したい物理キーを入れる
		bool IsActionPressImpl(int _action); // Pressの内部実装
		bool IsActionPushedImpl(int _action); // Pushedの内部実装
		bool IsActionReleasedImpl(int _action); // Releasedの内部実装
	}

	/// <summary>
	/// 物理入力を抽象化するアクションシステムを初期化する関数
	/// 各アクションの定義はenum classを自作してください
	/// その際に最後尾にはそのenum classの要素数を表すCountなどの要素を入れてください
	/// </summary>
	/// <typeparam name="TAction">各アクションを定義したenum class</typeparam>
	/// <param name="_count">アクションが定義されているenum classの最後尾にある要素数を表す部分</param>
	template<typename TAction>
	void SetupActions(TAction _count)
	{
		static_assert(std::is_enum_v<TAction>, "Actionはenum classで定義してください\n");
		Detail::SetupActionImpl(static_cast<int>(_count));
	}

	/// <summary>
	/// 指定したアクションに物理操作を紐づける関数
	/// 自作したenum classに指定したい操作を入れてください
	/// </summary>
	/// <typeparam name="TAction">自作したenum class</typeparam>
	/// <param name="_action">自作したenum classの指定アクション</param>
	/// <param name="_binding">設定したい物理操作</param>
	template<typename TAction>
	void SetAction(TAction _action, Binding _binding)
	{
		static_assert(std::is_enum_v<TAction>, "Actionはenum classで定義してください\n");
		Detail::SetActionImpl(static_cast<int>(_action), _binding);
	}

	// 抽象化 : 押している間
	template<typename TAction>
	bool IsActionPress(TAction _action)
	{
		static_assert(std::is_enum_v<TAction>, "Actionはenum classで定義してください\n");
		return Detail::IsActionPressImpl(static_cast<int>(_action));
	}
	// 抽象化 : 押した瞬間
	template<typename TAction>
	bool IsActionPushed(TAction _action)
	{
		static_assert(std::is_enum_v<TAction>, "Actionはenum classで定義してください\n");
		return Detail::IsActionPushedImpl(static_cast<int>(_action));
	}
	// 抽象化 : 離した瞬間
	template<typename TAction>
	bool IsActionReleased(TAction _action)
	{
		static_assert(std::is_enum_v<TAction>, "Actionはenum classで定義してください\n");
		return Detail::IsActionReleasedImpl(static_cast<int>(_action));
	}

	 // キーボード : 押している間
	bool IsKeyPress(KeyCode::Button _key);
	// キーボード : 押した瞬間
	bool IsKeyPushed(KeyCode::Button  _key);
	// キーボード : 離した瞬間
	bool IsKeyReleased(KeyCode::Button  _key);

	// マウス : 押している間
	bool IsMousePress(MouseCode::Click _click);
	// マウス : 押した瞬間
	bool IsMousePushed(MouseCode::Click _click);
	// マウス : 離した瞬間
	bool IsMouseReleased(MouseCode::Click _click);
	/// <summary>
	/// そのフレーム中のホイール回転量を基準値を含めて計算し、加算した値を返す
	/// 回転量 * 基準値の120を返す
	/// 回転方向は奥が+、手前が-
	/// </summary>
	/// <returns>1フレーム内の回転量 * 基準値加算</returns>
	int GetMouseWheelValue();
	/// <summary>
	/// そのフレーム中の回転数を加算して返す
	/// 回転方向は奥が+、手前が-
	/// </summary>
	/// <returns>回転量</returns>
	int GetMouseWheelNotchValue();
	// マウス : カーソルの座標を取得する(クライアント座標で左上が原点のY座標が下向きで単位はピクセル。画面外にカーソルが出ても負や画面越えのサイズとして出力します)
	Vector2Int GetMousePoint();
	// 前のフレームからのマウスの移動量を返す(変化量はフレームレートに依存します)
	Vector2Int GetMouseDelta();
	
	 // ゲームパッド : 押している間
	bool IsPadPress(PadCode::Button _key);
	bool IsPadPress(PadCode::Trigger _trigger);
	// ゲームパッド : 押した瞬間
	bool IsPadPushed(PadCode::Button _key);
	bool IsPadPushed(PadCode::Trigger _trigger);
	// ゲームパッド : 離した瞬間
	bool IsPadReleased(PadCode::Button _key);
	bool IsPadReleased(PadCode::Trigger _trigger);
	/// <summary>
	/// ゲームパッド : トリガー値(0-1に正規化した値を返す)
	/// 内部に用いている閾値が未押下30押下10なので
	/// 押されていても10-29の値であればGetTriggerValueは0.0を返す
	/// また上記の条件からGetPadTriggerValueが0より大きい -> IsPadPress = trueとなります
	/// </summary>
	/// <param name="_trigger">左右トリガーの選択</param>
	/// <returns>0-1に正規化されたトリガー値</returns>
	float GetPadTriggerValue(PadCode::Trigger _trigger);
	/// <summary>
	/// ゲームパッド : スティック値 引数: 左右, Y軸値を反転するかどうか
	/// 値域: 各成分およそ-1〜+1、デッドゾーン内はVector2::Zero
	/// 向き: デフォルト(false)は上に倒すとyが負（スクリーン座標系）。trueで反転し上が正
	/// </summary>
	/// <param name="_stick">左右スティックの選択</param>
	/// <param name="_isInverseY">Y軸反転を行うかどうか</param>
	/// <returns>正規化された入力ベクトル</returns>
	Vector2 GetPadStickValue(PadCode::Stick _stick, bool _isInverseY = false);
}