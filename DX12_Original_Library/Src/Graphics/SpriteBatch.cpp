#include <cmath>
#include <vector>
#include "../Debug/DebugLogs.h"
#include "GraphicsConstant.h"
#include "GraphicsDevice.h"
#include "DescriptorManager.h"
#include "GraphicsResourceManager.h"
#include "SpriteBatch.h"

namespace
{
	// 2つのMaterialパラメータ集合が同じGPUデータか調べるヘルパー
	bool IsSameMaterialParameters(const MaterialParameterSet& _lhs, const MaterialParameterSet& _rhs)
	{
		for (size_t i = 0; i < MATERIAL_PARAMETER_SLOT_COUNT; i++)
		{
			const MaterialParameterBlock& lhs{ _lhs[i] };
			const MaterialParameterBlock& rhs{ _rhs[i] };

			if (lhs.hasParameter != rhs.hasParameter) return false; // フラグはあっているか(設定状態化未設定か)
			if (lhs.parameterSize != rhs.parameterSize) return false; // サイズは一致しているか
			if (!lhs.hasParameter) continue; // 両方未設定ならこのスロットの中身を見る必要はないのでスキップ
			if (lhs.parameterData != rhs.parameterData) return false;
		}
		return true;
	}
}


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
	for (UINT i = 0; i < FRAME_BUFFER_COUNT; i++)
	{
		vertBuffers[i] = GraphicsResourceManager::Instance().CreateDynamicVertexBuffer(nullptr, MAX_SPRITE_COUNT * 4 * sizeof(SpriteVertex), sizeof(SpriteVertex)); // 動的な頂点バッファの作成
	}
}

void SpriteBatch::Shutdown()
{
	for (VertexBuffer& buffer : vertBuffers)
	{
		buffer = VertexBuffer{};
	}
	indexBuffer = IndexBuffer{};
	spriteCounter = 0;
	droppedCounter = 0;
	runs = std::vector<SpriteDrawRun>{}; // vectorの確保容量も返す
	rootSig = nullptr;
	pipelineState = nullptr;
	gpuVirtualAddres = nullptr;
}

void SpriteBatch::RegisterSprite(TexHandle _handle, ID3D12PipelineState* _pipelineState, const MaterialParameterSet* _parameters, Vector2 _position, Vector2 _size, float _radRotation, Vector4 _color, Vector2 _uvMin, Vector2 _uvMax)
{
	if (!_handle.IsValid())
	{
		// DEBUG_LOG_WARNING("無効ハンドルが渡されました\n");
		// 現状単一スレッドのためAssertにしているがマルチスレッドにしたらそれ専用の待機にする
		DEBUG_ASSERT(_handle.IsValid() && "無効ハンドルが渡されました\n");
		return; // 無効ハンドルか
	}

	if (!_pipelineState)
	{
		DEBUG_ASSERT(_pipelineState != nullptr && "Sprite用PSOがnullです\n");
		return;
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

	// 今描画しているBackBufferと同じ番号の頂点バッファへ書き込む
	const UINT frameIndex{ GraphicsDevice::Instance().GetCurrentFrameIndex() };
	const float COLOR[4]{ _color.x, _color.y, _color.z, _color.w }; // 色をfloatに
	SpriteVertex* vertices{ static_cast<SpriteVertex*>(vertBuffers[frameIndex].mappedPtr)}; // マップされたポインタにアクセスするためにキャスト
	
	// UV空間をハードコーディングするのではなく引数から受け取る形に変更
	vertices[spriteCounter * 4 + 0] = { {leftTop.x, leftTop.y, 0.0f}, {_uvMin.x, _uvMin.y}, { COLOR[0], COLOR[1], COLOR[2], COLOR[3] } }; // 左上
	vertices[spriteCounter * 4 + 1] = { {rightTop.x, rightTop.y, 0.0f}, {_uvMax.x, _uvMin.y}, { COLOR[0], COLOR[1], COLOR[2], COLOR[3] } }; // 右上
	vertices[spriteCounter * 4 + 2] = { {rightBottom.x, rightBottom.y, 0.0f}, {_uvMax.x, _uvMax.y}, { COLOR[0], COLOR[1], COLOR[2], COLOR[3] } }; // 右下
	vertices[spriteCounter * 4 + 3] = { {leftBottom.x, leftBottom.y, 0.0f}, {_uvMin.x, _uvMax.y}, { COLOR[0], COLOR[1], COLOR[2], COLOR[3] } }; // 左下

	// 内蔵のSpriteなどのMaterialを使わない空の場合のデータ
	static const MaterialParameterSet EMPTY_PARAMETERS{};
	const MaterialParameterSet& useParameters{ _parameters ? *_parameters : EMPTY_PARAMETERS };

	// run(描画順)を管理する
	// 空、一つ前とハンドルが違う、一つ前とPSOが違う、一つ前とパラメータが違う条件で新しく描画区間を作る
	if (runs.empty() || runs.back().tex != _handle || runs.back().pipelineState != _pipelineState || !IsSameMaterialParameters(runs.back().parameters, useParameters))
	{
		SpriteDrawRun run{};
		run.tex = _handle;
		run.pipelineState = _pipelineState;
		run.parameters = useParameters; // この時点の値をコピー
		run.startSprite = spriteCounter;
		run.count = 1;
		runs.push_back(std::move(run));
	}
	else
	{
		// 同一テクスチャ同一PSO同一パラメータなら同じドローコールへ
		runs.back().count++;
	}

	spriteCounter++; // カウンターを増加する
}

void SpriteBatch::Flush(RingConstantBuffer& _parameterRing, ID3D12Resource* _zeroParameterBuffer)
{
	if (runs.empty()) return; // 何もなければパイプライン設定などもせずに即return
	if (!_zeroParameterBuffer)
	{
		DEBUG_LOG_ERROR("SpriteBatchへ渡されたゼロダミーCBがnullです\n");
		runs.clear();
		return;
	}
	auto* cmd{GraphicsDevice::Instance().GetCommandList()}; // コマンドリストをキャッシュ
	// 今描画しているBackBufferと同じ番号の頂点バッファへ書き込む
	const UINT frameIndex{ GraphicsDevice::Instance().GetCurrentFrameIndex() };

	// パイプライン設定
	cmd->SetGraphicsRootSignature(rootSig);
	cmd->SetGraphicsRootConstantBufferView(1, gpuVirtualAddres->GetGPUVirtualAddress());

	// SRVが入っているDescriptorHeapをGPUにセットする
	DescriptorManager::Instance().SetDiscriptor(cmd);

	// 入力アセンブラを設定
	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST); // リスト設定
	cmd->IASetVertexBuffers(0, 1, &vertBuffers[frameIndex].vertexView);
	cmd->IASetIndexBuffer(&indexBuffer.indexView);

	GraphicsResourceManager& resourceManager{ GraphicsResourceManager::Instance() };

	// ランごとにSRVの差し替えとDrawを行う
	for (const SpriteDrawRun& run : runs)
	{
		TextureData* data{ GraphicsResourceManager::Instance().Lookup(run.tex) }; // ハンドルを分解して保持
		if (!data)
		{
			DEBUG_LOG_WARNING("Sprite描画前にテクスチャが無効になったため、エラーテクスチャへ差し替えます\n");
			data = resourceManager.Lookup(resourceManager.GetErrorTexture());
		}
		if (!data) continue; //エラーテクスチャまで取得できない場合は描画不可能
		if (!run.pipelineState) continue;
		cmd->SetPipelineState(run.pipelineState); // この区間で使用する外部または内部PSO
		cmd->SetGraphicsRootDescriptorTable(0, data->srvHandle.gpu); // ルートシグネチャの0番にテクスチャのGPUハンドルをセット
		
		constexpr UINT SPRITE_MATERIAL_ROOT_PARAM_BASE{ 2 }; // MaterialslotはRootParam[2]から始まる
		const D3D12_GPU_VIRTUAL_ADDRESS zeroParamterAddress{_zeroParameterBuffer->GetGPUVirtualAddress()};
		
		for (size_t i = 0; i < MATERIAL_PARAMETER_SLOT_COUNT; i++)
		{
			// 未設定の場合はゼロダミーCBを使用する
			D3D12_GPU_VIRTUAL_ADDRESS parameterAddress{ zeroParamterAddress };
			const MaterialParameterBlock& parameter{ run.parameters[i] };
			if (parameter.hasParameter)
			{
				// DrawSprite登録時に保存したスナップショットを送信する
				const D3D12_GPU_VIRTUAL_ADDRESS updatedAddress{ _parameterRing.Update(parameter.parameterData.data(), static_cast<UINT>(parameter.parameterSize)) };
				if (updatedAddress != 0)
				{
					parameterAddress = updatedAddress;
				}
				else
				{
					// Ring上限超過時も不正アドレスを設定しない
					DEBUG_LOG_ERROR("Sprite MaterialParameterのGPU転送に失敗しました slot = {}\n", i);
				}
			}
			cmd->SetGraphicsRootConstantBufferView(SPRITE_MATERIAL_ROOT_PARAM_BASE + static_cast<UINT>(i), parameterAddress);
		}
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