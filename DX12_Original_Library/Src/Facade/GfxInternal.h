#pragma once
#include <windows.h>
#include <functional>
#include <vector>
#include "../Graphics/GraphicsType.h"
// グラフィックに関する関数のInitialize等ユーザーに提供しない部分をまとめた関数
namespace GfxInternal
{
	// 初期化(ウィンドウのタイトルと幅と高さを設定)
	bool Initialize(const wchar_t* _title, int _width, int _height);
	// フレームの開始処理
	void BeginFrame();
	// フレームの終了処理
	void EndFrame();
	// 終了処理
	void Finish();


	// HWNDの取得(今後はsystemファサードの役目になる)
	HWND GetHWND();
	// ホイール関数のセット(今後はsystemファサードの役目になる)
	void SetOnWheel(std::function<void(short)> _func);
}