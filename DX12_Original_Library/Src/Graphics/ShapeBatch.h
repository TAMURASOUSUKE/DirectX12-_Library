#pragma once
#include <vector>
#include "../Math/TSMath.h"
#include "GraphicsType.h"

struct ShapeDrawRun
{
	bool isWireframe; // このランは枠線か(ランの境界基準)
	UINT startVertex{ 0 }; // このランがVBのどこから始まるか
	UINT count{ 0 }; // このランの頂点数
};

// 基礎図形のバッチング処理を管理するクラス
class ShapeBatch
{
public:
	// 初期化処理
	void Initialize(ID3D12RootSignature* _rootSig, ID3D12PipelineState* _fillState, ID3D12PipelineState* _wireState ,ID3D12Resource* _gpuVirtualAddres);

	// 基本図形の登録
	void RegisterBox(Vector2 _leftTop, Vector2 _rightBottom, float _radRotation = 0.0f, Vector4 _color = { 1.0f, 1.0f, 1.0f ,1.0f },bool _isWireframe = false); // 矩形
	void RegisterCircle(Vector2 _center, float _radius, Vector4 _color = { 1.0f, 1.0f, 1.0f ,1.0f },  bool _isWireframe = false); // 円
	void RegisterCapsule(Vector2 _startPos, Vector2 _endPos, float _radius, Vector4 _color = { 1.0f, 1.0f, 1.0f ,1.0f }, bool _isWireframe = false); // カプセル
	void RegisterLine(Vector2 _startPos, Vector2 _endPos, Vector4 _color = { 1.0f, 1.0f, 1.0f ,1.0f }); // 線分

	// まとめて流す
	void Flush();

	// カウンター等をリセットする
	void Reset();
private:
	VertexBuffer vertBuffer; // 頂点バッファ
	// 一旦インデックスバッファは使わずに作成する = 簡単な図形のため影響が少ない(今後拡張してインデックスを使う)
	UINT shapeCounter{ 0 }; // 今のフレームにどれだけ基礎図形が登録されているか
	UINT shapeVertexCounter{ 0 }; // 頂点数をカウントする(次の開始位置を求める物として使う)
	UINT droppedCounter{ 0 }; // あふれた基礎図形のカウンター
	std::vector<ShapeDrawRun> runs; // 描画順をまとめた配列

	// 外部から受け取るパラメータ(Flush時にパイプライン設定などを行うため)
	ID3D12RootSignature* rootSig{ nullptr }; // ルートシグネチャ
	ID3D12PipelineState* wireState{ nullptr }; // wireのPSO
	ID3D12PipelineState* fillState{ nullptr }; // 塗りつぶしのPSO
	ID3D12Resource* gpuVirtualAddres{ nullptr }; // 定数バッファの仮想GPUアドレスを取得するための変数
};