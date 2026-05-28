#pragma once


// グラフィックスに関する機能をユーザーに簡易的に提供するためのファイル
namespace Gfx 
{
	bool Initialize(const wchar_t* _title, int _width, int _height); // 初期化(ウィンドウのタイトルと幅と高さを設定)
	bool ProcessMessage(); // メッセージループ
	void BeginFrame(); // フレームの開始処理
	void EndFrame(); // フレームの終了処理
	void Finish(); // 終了処理
	void ClearScreen(float _r = 0.0f, float _g = 0.0f, float _b = 0.0f, float _a = 1.0f); // 画面のクリア(引数で色を設定できるデフォルトは黒)
	void DrawTriangle(); // 三角形描画
	void DrawTexture(); // テクスチャ描画
}