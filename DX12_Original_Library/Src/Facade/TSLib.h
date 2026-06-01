#pragma once
#include "Gfx.h"
#include "Input.h"
// 初期化などlibrary全体の機能をまとめてユーザーに提供する
namespace TSLib
{
	bool Initialize(const wchar_t* _title, int _width, int _height); // // 初期化(ウィンドウのタイトルと幅と高さを設定)
	void BeginFrame(); // フレームの開始処理
	void EndFrame(); // フレームの終了処理
	void Finish(); // 終了処理
}