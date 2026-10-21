#pragma once
#include <d3d12.h>
#include <wrl/client.h>
#include <vector>
#include "../Core/Handle/TexHandle.h"
#include "RingConstantBuffer.h"
#include "GraphicsType.h"
using Microsoft::WRL::ComPtr;


// Terrainによる地形描画を担当するシステム
class TerrainSystem
{
public:
	TerrainSystem() = default;
	~TerrainSystem() = default;

	TerrainSystem(const  TerrainSystem& _other) = delete;
	TerrainSystem& operator=(const TerrainSystem& _other) = delete;

	// 初期化関数
	bool Setup(ID3D12RootSignature* _rootSig, ID3D12PipelineState* _pipelineState);

	// 終了処理
	void Shutdown();

	// 描画命令を登録する
	void RegsterTerrain(Vector3 _position, float _scale, float _tessFactor, float _heightScale, Vector4 _color, TexHandle _heightMap, const Mat4x4& _viewProjectionMatrix);

	// 登録された描画命令を実行する
	void Flush(ID3D12GraphicsCommandList* _cmd);

	// フレーム開始の状態へリセットする
	void Reset();

	// 描画待機中のテラインはあるか
	bool HasPendingDraw() const { return !terrainContexts.empty(); }

private:
	// Terrainを描画する際のHeightMap,VirtualGPUAddressの情報をまとめて命令時に記録しFlush時に使う(計測時間を保証するため)
	struct TerrainContext
	{
		D3D12_GPU_VIRTUAL_ADDRESS gpuVirtualAddress{};
		TexHandle heightMap;
	};

	// グリッド生成の責任を持つ関数
	bool CreateGrid();

private:
	ComPtr<ID3D12RootSignature> rootSig{ nullptr }; // ルートシグネチャ
	ComPtr<ID3D12PipelineState> pipeline{ nullptr }; // PSO
	VertexBuffer vertexBuffer{}; // 頂点バッファ
	IndexBuffer indexBuffer{}; // インデックスバファ
	RingConstantBuffer ringCBV{}; // 描画に必要な定数バッファをリングバッファ管理に入れる
	std::vector<TerrainContext> terrainContexts; // テライン描画に必要な情報

	bool isInitialize{ false }; // すでに初期化されているか
};
