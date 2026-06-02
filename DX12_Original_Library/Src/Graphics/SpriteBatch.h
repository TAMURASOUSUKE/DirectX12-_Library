#pragma once
#include "../Math/TSMath.h"
#include "GraphicsType.h"

// SpriteBatchを実装し大量のスプライトを効率よく描画できるようにするためのクラス
class SpriteBatch
{
public:
	// 初期化
	void Initialize(ID3D12RootSignature* _rootSig, ID3D12PipelineState* _pipelineState, ID3D12Resource* _gpuVirtualAddres);
	// スプライトの登録(画像と位置とサイズ)
	void RegisterSprite(TextureData _srvHandle, Vector2 _position, Vector2 _size);
	// まとめてDrawCallをする
	void Flush();
	
private:

	VertexBuffer vertBuffer; // 頂点バッファ
	IndexBuffer indexBuffer; // インデックスバッファ
	UINT spriteCounter{ 0 }; // 今のフレームにどれだけスプライトが登録されているか
	DescriptorHandle currentBatchingTexture; // 現在batch中のテクスチャ

	// 外部から受け取るパラメータ(Flush時にパイプライン設定などを行うため)
	ID3D12RootSignature* rootSig; // ルートシグネチャ
	ID3D12PipelineState* pipelineState; // PSO
	ID3D12Resource* gpuVirtualAddres; // 定数バッファの仮想GPUアドレスを取得するための変数

};