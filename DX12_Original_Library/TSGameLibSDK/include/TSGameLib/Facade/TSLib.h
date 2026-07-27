#pragma once

// TSLibは現状Windows専用ライブラリなのでこのヘッダー1枚でWinMainに必要な型も使用可能にする
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

// windows.hのmin/maxマクロによるstd::min/std::maxとの衝突を防ぐ
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include "../Math/TSMath.h"
#include "../Collision/Collider.h"
#include "../Debug/DebugLogs.h"  
#include "TSTime.h"
#include "Gfx.h"
#include "Collision.h"
#include "Input.h"
#include "Sound.h"


// 初期化などlibrary全体の機能をまとめてユーザーに提供する
namespace TSLib
{
	// 初期化(ウィンドウのタイトルと幅と高さを設定)
	bool Initialize(const wchar_t* _title, int _width, int _height);
	// メッセージループ
	bool ProcessMessage();
	// フレームの開始処理
	void BeginFrame();
	// フレームの終了処理
	void EndFrame();
	// 終了処理
	void Finish();
}
