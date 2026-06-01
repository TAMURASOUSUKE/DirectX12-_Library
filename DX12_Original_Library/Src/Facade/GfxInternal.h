#pragma once

// グラフィックに関する関数のInitialize等ユーザーに提供しない部分をまとめた関数
namespace GfxInternal
{
	bool Initialize(const wchar_t* _title, int _width, int _height); // 初期化(ウィンドウのタイトルと幅と高さを設定)
	void BeginFrame(); // フレームの開始処理
	void EndFrame(); // フレームの終了処理
	void Finish(); // 終了処理
}