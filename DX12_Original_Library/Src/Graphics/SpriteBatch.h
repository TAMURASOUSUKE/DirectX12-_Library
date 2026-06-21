#pragma once
#include "../Core/Handle/TexHandle.h"
#include "../Math/TSMath.h"
#include "GraphicsType.h"

// 描画順を記録するためのもの
struct DrawRun
{
	TexHandle tex; // ハンドル
	UINT startSprite; // スタート位置
	UINT count; // 同じテクスチャが何枚連続しているか
};

// SpriteBatchを実装し大量のスプライトを効率よく描画できるようにするためのクラス
class SpriteBatch
{
public:
	// 初期化
	void Initialize(ID3D12RootSignature* _rootSig, ID3D12PipelineState* _pipelineState, ID3D12Resource* _gpuVirtualAddres);
	// スプライトの登録(画像と位置とサイズと回転角度とUV空間)
	void RegisterSprite(TexHandle _handle, Vector2 _position, Vector2 _size, float _radRotation = 0.0f, Vector2 _uvMin = { Vector2::Zero }, Vector2 _uvMax = { Vector2::One });
	// まとめてDrawCallをする
	void Flush();
	// カウンター等をリセットする
	void Reset();

	
private:

	VertexBuffer vertBuffer; // 頂点バッファ
	IndexBuffer indexBuffer; // インデックスバッファ
	UINT spriteCounter{ 0 }; // 今のフレームにどれだけスプライトが登録されているか
	UINT droppedCounter{ 0 }; // あふれた画像数のカウンター
	std::vector<DrawRun> runs; // 描画順をまとめた配列

	// 外部から受け取るパラメータ(Flush時にパイプライン設定などを行うため)
	ID3D12RootSignature* rootSig{ nullptr }; // ルートシグネチャ
	ID3D12PipelineState* pipelineState{ nullptr }; // PSO
	ID3D12Resource* gpuVirtualAddres{ nullptr }; // 定数バッファの仮想GPUアドレスを取得するための変数

};