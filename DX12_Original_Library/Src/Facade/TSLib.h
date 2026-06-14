#pragma once
#include "../Math/TSMath.h"
#include "../Core/LogAssert/Debug.h"  
#include "Gfx.h"
#include "Input.h"
// 初期化などlibrary全体の機能をまとめてユーザーに提供する
namespace TSLib
{
	// 初期化(ウィンドウのタイトルと幅と高さを設定)
	bool Initialize(const wchar_t* _title, int _width, int _height);
	// フレームの開始処理
	void BeginFrame();
	// フレームの終了処理
	void EndFrame();
	// 終了処理
	void Finish();
}