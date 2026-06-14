#include <cmath>
#include <vector>
#include "GraphicsConstant.h"
#include "GraphicsDevice.h"
#include "DescriptorManager.h"
#include "ResourceManager.h"
#include "SpriteBatch.h"

void SpriteBatch::Initialize(ID3D12RootSignature* _rootSig, ID3D12PipelineState* _pipelineState, ID3D12Resource* _gpuVirtualAddres)
{
	if (_rootSig == nullptr || _pipelineState == nullptr || _gpuVirtualAddres == nullptr) return;
	// 各パラメータと繋げる
	rootSig = _rootSig;
	pipelineState = _pipelineState;
	gpuVirtualAddres = _gpuVirtualAddres;

	// インデックス配列
	std::vector<UINT> indexArray(MAX_SPRITE_COUNT * 6); // 頂点数をかける

	// 頂点用ループ
	for (int i = 0; i < MAX_SPRITE_COUNT; i++)
	{
		UINT base{ static_cast<UINT>(i) * 4 }; // 頂点はスプライトごと4ずつ増えるので頂点番号はN * 4(頂点の開始番号)
		UINT offset{ static_cast<UINT>(i) * 6 }; // 配列の書き込み位置
		indexArray[offset + 0] = base + 0;
		indexArray[offset + 1] = base + 1;
		indexArray[offset + 2] = base + 2;
		indexArray[offset + 3] = base + 0;
		indexArray[offset + 4] = base + 2;
		indexArray[offset + 5] = base + 3;
	}

	indexBuffer = ResourceManager::Instance().CreateIndexBuffer(indexArray.data(), indexArray.size() * sizeof(UINT), MAX_SPRITE_COUNT * 6); // インデックスバッファの作成
	vertBuffer = ResourceManager::Instance().CreateDynamicVertexBuffer(nullptr, MAX_SPRITE_COUNT * 4 * sizeof(TexVertex), sizeof(TexVertex)); // 動的な頂点バッファの作成
}

void SpriteBatch::RegisterSprite(TexHandle _handle, Vector2 _position, Vector2 _size, float _radRotation)
{
	if (!_handle.IsValid()) return; // 無効ハンドルか
	if (spriteCounter >= MAX_SPRITE_COUNT) return; // 限界を超えているならreturn
	
	// 回転の適用
	Vector2 leftUp{ _position.x, _position.y }; // 左上
	Vector2 rightUp{ _position.x + _size.x, _position.y }; // 右上
	Vector2 rightDown{ _position.x + _size.x, _position.y + _size.y }; // 右下
	Vector2 leftDown{ _position.x, _position.y + _size.y }; // 左下
	if (_radRotation != 0.0f)
	{
		Vector2 center{ _position + _size / 2.0f }; // 中心
		// 相対座標を適用
		leftUp -= center;
		rightUp -= center;
		rightDown -= center;
		leftDown -= center;

		float c{ std::cosf(_radRotation) }; // cosθ
		float s{ std::sinf(_radRotation) }; // sinθ

		leftUp = { leftUp.x * c - leftUp.y * s, leftUp.x * s + leftUp.y * c };
		rightUp = { rightUp.x * c - rightUp.y * s, rightUp.x * s + rightUp.y * c };
		rightDown = { rightDown.x * c - rightDown.y * s, rightDown.x * s + rightDown.y * c };
		leftDown = { leftDown.x * c - leftDown.y * s, leftDown.x * s + leftDown.y * c };

		leftUp = leftUp + center;
		rightUp = rightUp + center;
		rightDown = rightDown + center;
		leftDown = leftDown + center;
	}

	// 現在のテクスチャと異なるなら
	if (currentBatchingTexture != _handle)
	{
		Flush();
	}

	TexVertex* vertices{ static_cast<TexVertex*>(vertBuffer.mappedPtr) }; // マップされたポインタにアクセスするためにキャスト
	vertices[spriteCounter * 4 + 0] = { {leftUp.x, leftUp.y, 0.0f}, {0.0f, 0.0f} }; // 左上
	vertices[spriteCounter * 4 + 1] = { {rightUp.x, rightUp.y, 0.0f}, {1.0f, 0.0f} }; // 右上
	vertices[spriteCounter * 4 + 2] = { {rightDown.x, rightDown.y, 0.0f}, {1.0f, 1.0f} }; // 右下
	vertices[spriteCounter * 4 + 3] = { {leftDown.x, leftDown.y, 0.0f}, {0.0f, 1.0f} }; // 左下
	spriteCounter++; // カウンターを増加する
	currentBatchingTexture = _handle;
}

void SpriteBatch::Flush()
{
	if (spriteCounter == batchStart) return; // 登録されている画像数がbatch開始位置とかぶっているなら即retrun

	TextureData* data{ ResourceManager::Instance().Lookup(currentBatchingTexture) }; // 現在のハンドル内のデータ取り出し

	// 無効なハンドルの場合
	if (!data)
	{
		// ゴミを残さないためにnullの時はこのbatchを捨てて次へ行く
		batchStart = spriteCounter;
		return;
	}

	// パイプライン設定
	GraphicsDevice::Instance().GetCommandList()->SetGraphicsRootSignature(rootSig);
	GraphicsDevice::Instance().GetCommandList()->SetGraphicsRootConstantBufferView(1, gpuVirtualAddres->GetGPUVirtualAddress());
	GraphicsDevice::Instance().GetCommandList()->SetPipelineState(pipelineState);

	// SRVが入っているDescriptorHeapをGPUにセットする
	DescriptorManager::Instance().SetDiscriptor(GraphicsDevice::Instance().GetCommandList());

	// ルートシグネチャの0番にテクスチャのGPUハンドルをセット
	GraphicsDevice::Instance().GetCommandList()->SetGraphicsRootDescriptorTable(0, data->srvHandle.gpu);

	// 入力アセンブラを設定
	GraphicsDevice::Instance().GetCommandList()->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST); // リスト設定
	GraphicsDevice::Instance().GetCommandList()->IASetVertexBuffers(0, 1, &vertBuffer.vertexView);
	GraphicsDevice::Instance().GetCommandList()->IASetIndexBuffer(&indexBuffer.indexView);

	// インデックス描画(登録されているインデックス分だけ描画)
	GraphicsDevice::Instance().GetCommandList()->DrawIndexedInstanced((spriteCounter - batchStart) * 6, 1, 0, batchStart * 4, 0);

	// 画像を切り替えたときに頂点を上書きしないようにするためにbatchのスタート位置を決定する
	batchStart = spriteCounter;
}

// 0リセットを入れる
void SpriteBatch::Reset()
{
	spriteCounter = 0;
	batchStart = 0;
}