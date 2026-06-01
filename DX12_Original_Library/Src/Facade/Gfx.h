#pragma once


// グラフィックスに関する機能をユーザーに簡易的に提供するためのファイル
namespace Gfx 
{
	bool ProcessMessage(); // メッセージループ
	void ClearScreen(float _r = 0.0f, float _g = 0.0f, float _b = 0.0f, float _a = 1.0f); // 画面のクリア(引数で色を設定できるデフォルトは黒)
	void DrawTriangle(); // 三角形描画
	void DrawTexture(); // テクスチャ描画
}