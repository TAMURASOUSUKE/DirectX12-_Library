#pragma once
#include <windows.h>
#include "../Math/TSMath.h"
#include "InputBase.h"

// マウスの入力を簡易化するクラス
class MouseInput : public InputBase
{
public:
	void Setup(HWND _hwnd);  // 初期化

	void Update() override; // 入力更新

	bool IsPress(int _click) override; // 押している間
	bool IsPushed(int _click) override; // 押した瞬間
	bool IsReleased(int _click) override; // 離した瞬間

	// このフレーム操作が発生したか
	bool IsInputActiveThisFrame();

	/// <summary>
	/// そのフレーム中のホイール回転量を基準値を含めて計算し、加算した値を返す
	/// 回転量 * 基準値の120を返す
	/// 回転方向は奥が+、手前が-
	/// </summary>
	/// <returns>1フレーム内の回転量 * 基準値加算</returns>
	int GetWheelValue();

	/// <summary>
	/// そのフレーム中の回転数を加算して返す
	/// 回転方向は奥が+、手前が-
	/// </summary>
	/// <returns>回転量</returns>
	int GetWheelNotchValue();

	// 左上原点のy軸下向きのクライアント座標を返す。(単位ピクセル)
	Vector2Int GetCursorPoint();

	// 前のフレームからのマウスの移動量を返す
	Vector2Int GetCursorDelta();

	// カーソルを強制移動した後追跡基準を現在位置に合わせる
	bool ResetCursorTracking();

	// wheelが回された分だけ加算して積む。
	void AddWheelDelta(short _delta);
private:
	HWND hwnd{}; // カーソル用ウィンドウハンドル
	BYTE currentClicks[256]{};
	BYTE prevClicks[256]{};
	POINT currentClientCursorPos{}; // 現在のマウスカーソル位置
	POINT prevClientCursorPos{}; // 前フレームのマウスカーソル位置
	Vector2Int cursorDelta{ Vector2Int::Zero }; // カーソル移動量
	short accumWheel{ 0 }; // wheelを動作したときに得られる値の加算器 
	int currentWheel{ 0 }; // 最終的なユーザー側に出力するwheel稼働の値(ノッチ数)
};
