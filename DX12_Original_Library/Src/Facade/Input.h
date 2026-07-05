#pragma once
#include "../Math/TSMath.h"
#include "../Input/InputName.h"

// 入力に関する機能をユーザーに提供する
namespace Input
{
	// 抽象化 : 押している間
	bool IsPress(int _key); 
	// 抽象化 : 押した瞬間
	bool IsPushed(int _key);
	// 抽象化 : 離した瞬間
	bool IsReleased(int _key);

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
	// 前のフレームからのマウスの移動量を返す
	Vector2 GetMouseDelta();
	
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