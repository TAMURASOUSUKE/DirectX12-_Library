#include <cmath>
#include <vector>
#include "../Debug/DebugLogs.h"
#include "GraphicsConstant.h"
#include "GraphicsDevice.h"
#include "DescriptorManager.h"
#include "GraphicsResourceManager.h"
#include "SpriteBatch.h"

void SpriteBatch::Initialize(ID3D12RootSignature* _rootSig, ID3D12PipelineState* _pipelineState, ID3D12Resource* _gpuVirtualAddres)
{
	DEBUG_ASSERT(_rootSig != nullptr && _pipelineState != nullptr && _gpuVirtualAddres != nullptr);
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

	indexBuffer = GraphicsResourceManager::Instance().CreateIndexBuffer(indexArray.data(), static_cast<UINT>(indexArray.size()) * sizeof(UINT), MAX_SPRITE_COUNT * 6); // インデックスバッファの作成
	vertBuffer = GraphicsResourceManager::Instance().CreateDynamicVertexBuffer(nullptr, MAX_SPRITE_COUNT * 4 * sizeof(TexVertex), sizeof(TexVertex)); // 動的な頂点バッファの作成
}

void SpriteBatch::Shutdown()
{
	vertBuffer = VertexBuffer{};
	indexBuffer = IndexBuffer{};
	spriteCounter = 0;
	droppedCounter = 0;
	runs = std::vector<SpriteDrawRun>{}; // vectorの確保容量も返す
	rootSig = nullptr;
	pipelineState = nullptr;
	gpuVirtualAddres = nullptr;
}

void SpriteBatch::RegisterSprite(TexHandle _handle, Vector2 _position, Vector2 _size, float _radRotation, Vector2 _uvMin, Vector2 _uvMax)
{
	if (!_handle.IsValid())
	{
		// DEBUG_LOG_WARNING("無効ハンドルが渡されました\n");
		// 現状単一スレッドのためAssertにしているがマルチスレッドにしたらそれ専用の待機にする
		DEBUG_ASSERT(_handle.IsValid());
		return; // 無効ハンドルか
	}

	if (spriteCounter >= MAX_SPRITE_COUNT)
	{
		droppedCounter++; // あふれているならカウントする
		return; // 限界を超えているならreturn
	}
	
	// 回転の適用
	Vector2 leftTop{ _position.x, _position.y }; // 左上
	Vector2 rightTop{ _position.x + _size.x, _position.y }; // 右上
	Vector2 rightBottom{ _position.x + _size.x, _position.y + _size.y }; // 右下
	Vector2 leftBottom{ _position.x, _position.y + _size.y }; // 左下
	if (_radRotation != 0.0f)
	{
		Vector2 center{ _position + _size / 2.0f }; // 中心
		// 相対座標を適用
		leftTop -= center;
		rightTop -= center;
		rightBottom -= center;
		leftBottom -= center;

		float c{ std::cosf(_radRotation) }; // cosθ
		float s{ std::sinf(_radRotation) }; // sinθ

		leftTop = { leftTop.x * c - leftTop.y * s, leftTop.x * s + leftTop.y * c };
		rightTop = { rightTop.x * c - rightTop.y * s, rightTop.x * s + rightTop.y * c };
		rightBottom = { rightBottom.x * c - rightBottom.y * s, rightBottom.x * s + rightBottom.y * c };
		leftBottom = { leftBottom.x * c - leftBottom.y * s, leftBottom.x * s + leftBottom.y * c };

		leftTop = leftTop + center;
		rightTop = rightTop + center;
		rightBottom = rightBottom + center;
		leftBottom = leftBottom + center;
	}

	TexVertex* vertices{ static_cast<TexVertex*>(vertBuffer.mappedPtr) }; // マップされたポインタにアクセスするためにキャスト
	
	// UV空間をハードコーディングするのではなく引数から受け取る形に変更
	vertices[spriteCounter * 4 + 0] = { {leftTop.x, leftTop.y, 0.0f}, {_uvMin.x, _uvMin.y} }; // 左上
	vertices[spriteCounter * 4 + 1] = { {rightTop.x, rightTop.y, 0.0f}, {_uvMax.x, _uvMin.y} }; // 右上
	vertices[spriteCounter * 4 + 2] = { {rightBottom.x, rightBottom.y, 0.0f}, {_uvMax.x, _uvMax.y} }; // 右下
	vertices[spriteCounter * 4 + 3] = { {leftBottom.x, leftBottom.y, 0.0f}, {_uvMin.x, _uvMax.y} }; // 左下

	// run(描画順)を管理する
	if (runs.empty() || runs.back().tex != _handle)
	{
		// 配列が空もしくは一番最後のハンドルが登録しようとしているハンドルと異なるなら
		runs.push_back({_handle, spriteCounter, 1});
	}
	else
	{
		// 同一テクスチャならカウントを増やす
		runs.back().count++;
	}

	spriteCounter++; // カウンターを増加する
}

void SpriteBatch::Flush()
{
	if (runs.empty()) return; // 何もなければパイプライン設定などもせずに即return

	auto* cmd{GraphicsDevice::Instance().GetCommandList()}; // コマンドリストをキャッシュ

	// パイプライン設定
	cmd->SetGraphicsRootSignature(rootSig);
	cmd->SetGraphicsRootConstantBufferView(1, gpuVirtualAddres->GetGPUVirtualAddress());
	cmd->SetPipelineState(pipelineState);

	// SRVが入っているDescriptorHeapをGPUにセットする
	DescriptorManager::Instance().SetDiscriptor(cmd);

	// 入力アセンブラを設定
	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST); // リスト設定
	cmd->IASetVertexBuffers(0, 1, &vertBuffer.vertexView);
	cmd->IASetIndexBuffer(&indexBuffer.indexView);

	// ランごとにSRVの差し替えとDrawを行う
	for (const SpriteDrawRun& run : runs)
	{
		TextureData* data{ GraphicsResourceManager::Instance().Lookup(run.tex) }; // ハンドルを分解して保持
		if (!data) continue; // 無効ハンドルはスキップ
		cmd->SetGraphicsRootDescriptorTable(0, data->srvHandle.gpu); // ルートシグネチャの0番にテクスチャのGPUハンドルをセット
		cmd->DrawIndexedInstanced(run.count * 6, 1, 0, run.startSprite * 4, 0); // 区間情報から描画位置を特定して描画する(読むインデックスの数,  開始位置)
	}

	runs.clear(); // 消費したのでクリアする

}

// 0リセットを入れる
void SpriteBatch::Reset()
{
	if (droppedCounter > 0) DEBUG_LOG_WARNING("画像最大描画数を超過しました 超過枚数 : {}", droppedCounter);
	spriteCounter = 0;
	droppedCounter = 0;
	runs.clear(); // Flushで空になるがFlushなしで終わるケースの保険とする
}