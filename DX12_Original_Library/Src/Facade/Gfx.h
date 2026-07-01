#pragma once
#include "../Core/Handle/TexHandle.h"
#include "../Core/Handle/ModelHandle.h"
#include "../Graphics/GraphicsType.h"
#include "../Component/Transform.h"
#include "../Math/TSMath.h"

// グラフィックスに関する機能をユーザーに簡易的に提供するためのファイル
namespace Gfx 
{
	// フォントアトラスの設定構造体(デフォルトでフォントを用意しているが変更したい時にここを設定してもらう)
	struct BitmapFont
	{
		TexHandle texture; // アトラス画像のハンドル
		int texWidth{ 0 }; // テクスチャ全体の幅(px)
		int texHeight{ 0 }; // テクスチャ全体の高さ(px)
		int cellWidth{ 0 }; // 1セルの幅(px)
		int cellHeight{ 0 }; // 1セルの高さ(px)
		int cols{ 0 }; // 1行あたりのセルの量
		int firstCode{ 0 }; // 先頭セルが表す文字コード(CP437配列なら0, スペース始まりなら32)
	};

	// メッセージループ
	bool ProcessMessage();
	// 画面のクリア(引数で色を設定できるデフォルトは黒)
	void ClearScreen(float _r = 0.0f, float _g = 0.0f, float _b = 0.0f, float _a = 1.0f);
	//画像読み込み
	TexHandle LoadTexture(const char* _filePath);
	// モデル読み込み
	ModelHandle LoadModel(const char* _filePath);
	// 三角形描画
	void DrawTriangle();
	// テクスチャ描画
	void DrawTexture(); 
	// 矩形描画
	void DrawBox(Vector2 _leftTop, Vector2 _rightBottom, float _radRotation = 0.0f, Vector4 _color = { 1.0f, 1.0f, 1.0f ,1.0f }, bool _isWireframe = false);
	// 円描画
	void DrawCircle(Vector2 _center, float _radius, Vector4 _color = { 1.0f, 1.0f, 1.0f ,1.0f }, bool _isWireframe = false);
	// カプセル描画
	void DrawCapsule(Vector2 _startPos, Vector2 _endPos, float _radius, Vector4 _color = { 1.0f, 1.0f, 1.0f ,1.0f }, bool _isWireframe = false);
	// Cube描画(角度を渡すデフォルトは0°)
	void DrawCube(Vector3 _angle = Vector3::Zero);
	// 文字列描画 ; デフォルトフォント使用版(文字列, 位置, スケール(デフォルト1.0f), 描画レイヤー(デフォルト前面))
	void DrawString(const char* _string, Vector2 _position, float _scale = 1.0f, LenderLayer _layer = LenderLayer::ForeGround);
	// 文字描画 : 独自フォント使用版(フォント(構造体による別途設定必須), 文字列, 位置, スケール(デフォルト1.0f), 描画レイヤー(デフォルト前面))
	void DrawString(const BitmapFont& _font, const char* _string, Vector2 _position, float _scale = 1.0f, LenderLayer _layer = LenderLayer::ForeGround);
	// スプライト描画(位置、サイズ、画像, 回転角度(ラジアンかつデフォルトは0), uv座標(デフォルトは左上0右下1) 描画するレイヤー(デフォルトは通常 = 3Dより手前))
	void DrawSprite(TexHandle _texture, Vector2 _position, Vector2 _size, float _radRotation = 0.0f, Vector2 _uvMin = { Vector2::Zero }, Vector2 _uvMax = { Vector2::One }, LenderLayer _layer = LenderLayer::ForeGround);
	// モデルを描画する(テスト用にAnimDataを受け取っているが後で修正)
	void DrawModel(ModelHandle _model, Transform _transform, AnimInstanceData* _animData = nullptr);
	// 色の変更(今後は引数を変更)
	void SetBaseColor(ModelHandle model, int submeshIndex, Vector4 color);
	// テクスチャの変更(今後は引数を変更)
	void SetTexture(ModelHandle model, int submeshIndex, TexHandle texture);
	// テクスチャリソースの解放
	void Unload(TexHandle _handle);
	void Unload(ModelHandle _hanlde);
}