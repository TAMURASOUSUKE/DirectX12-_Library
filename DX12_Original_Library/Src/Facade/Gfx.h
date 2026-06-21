#pragma once
#include "../Core/Handle/TexHandle.h"
#include "../Graphics/GraphicsType.h"
#include "../Math/TSMath.h"

// グラフィックスに関する機能をユーザーに簡易的に提供するためのファイル
namespace Gfx 
{
	// メッセージループ
	bool ProcessMessage();
	// 画面のクリア(引数で色を設定できるデフォルトは黒)
	void ClearScreen(float _r = 0.0f, float _g = 0.0f, float _b = 0.0f, float _a = 1.0f);
	//画像読み込み
	TexHandle LoadTexture(const char* _filePath);
	// 三角形描画
	void DrawTriangle();
	// テクスチャ描画
	void DrawTexture(); 
	// Cube描画(角度を渡すデフォルトは0°)
	void DrawCube(Vector3 _angle = Vector3::Zero);
	// スプライト描画(位置、サイズ、画像, 回転角度(ラジアンかつデフォルトは0), uv座標(デフォルトは左上0右下1) 描画するレイヤー(デフォルトは通常 = 3Dより手前))
	void DrawSprite(TexHandle _texture, Vector2 _position, Vector2 _size, float _radRotation = 0.0f, Vector2 _uvMin = { Vector2::Zero }, Vector2 _uvMax = { Vector2::One }, LenderLayer _layer = LenderLayer::ForeGround);
	// テクスチャリソースの解放
	void Unload(TexHandle _handle);
}