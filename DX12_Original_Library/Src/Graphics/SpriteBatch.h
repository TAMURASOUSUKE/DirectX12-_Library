#pragma once
#include "../Core/Handle/TexHandle.h"
#include "../Math/TSMath.h"
#include "RingConstantBuffer.h"
#include "GraphicsType.h"

// 描画順を記録するためのもの
struct SpriteDrawRun
{
	TexHandle tex{}; // ハンドル
	ID3D12PipelineState* pipelineState{ nullptr }; // この区間で使用するPSO
	MaterialParameterSet parameters{}; // DrawSpriteが呼ばれた時点でのMateriualパラメータ
	UINT startSprite{ 0 }; // スタート位置
	UINT count{ 0 }; // 同じテクスチャが何枚連続しているか
};

// SpriteBatchを実装し大量のスプライトを効率よく描画できるようにするためのクラス
class SpriteBatch
{
public:
	// 初期化
	void Initialize(ID3D12RootSignature* _rootSig, ID3D12PipelineState* _pipelineState, ID3D12Resource* _gpuVirtualAddres);
	// 終了処理
	void Shutdown();
	// スプライトの登録(画像とパイプラインとパラメータと位置とサイズと回転角度とUV空間)
	void RegisterSprite(TexHandle _handle, ID3D12PipelineState* _pipelineState, const MaterialParameterSet* _parameters, Vector2 _position, Vector2 _size, float _radRotation = 0.0f, Vector4 _color = Vector4::One, Vector2 _uvMin = {Vector2::Zero}, Vector2 _uvMax = {Vector2::One});
	// まとめてDrawCallをする
	void Flush(RingConstantBuffer& _parameterRing, ID3D12Resource* _zeroParameterBuffer);
	// カウンター等をリセットする
	void Reset();

	
private:
	VertexBuffer vertBuffers[FRAME_BUFFER_COUNT]; // 頂点バッファ : BackBufferごとに動的頂点バッファを分けてGPUが読み込んでいるときにCPUが上書きしないため
	IndexBuffer indexBuffer; // インデックスバッファ
	UINT spriteCounter{ 0 }; // 今のフレームにどれだけスプライトが登録されているか
	UINT droppedCounter{ 0 }; // あふれた画像数のカウンター
	std::vector<SpriteDrawRun> runs; // 描画順をまとめた配列

	// 外部から受け取るパラメータ(Flush時にパイプライン設定などを行うため)
	ID3D12RootSignature* rootSig{ nullptr }; // ルートシグネチャ
	ID3D12PipelineState* pipelineState{ nullptr }; // PSO
	ID3D12Resource* gpuVirtualAddres{ nullptr }; // 定数バッファの仮想GPUアドレスを取得するための変数

};