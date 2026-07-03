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

	
	 // ゲームパッド : 押している間
	bool IsPadPress(PadCode::Button _key);
	// ゲームパッド : 押した瞬間
	bool IsPadPushed(PadCode::Button _key);
	// ゲームパッド : 離した瞬間
	bool IsPadReleased(PadCode::Button _key);
	/// <summary>
	/// ゲームパッド : スティック値 引数: 左右, Y軸値を反転するかどうか
	/// 値域: 各成分およそ-1〜+1、デッドゾーン内はVector2::Zero
	/// 向き: デフォルト(false)は上に倒すとyが負（スクリーン座標系）。trueで反転し上が正
	/// </summary>
	/// <param name="_sitick">左右スティックの選択</param>
	/// <param name="_isInverseY">Y軸反転を行うかどうか</param>
	/// <returns></returns>
	Vector2 GetPadStickValue(PadCode::Stick _sitick, bool _isInverseY = false);
}