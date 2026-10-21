#include <utility>
#include "GraphicsConstant.h"
#include "GraphicsResourceManager.h"
#include "../Debug/DebugLogs.h"
#include "GraphicsMetrics.h"
#include "DescriptorManager.h"
#include "TerrainSystem.h"

bool TerrainSystem::Setup(ID3D12RootSignature* _rootSig, ID3D12PipelineState* _pipelineState)
{
	if (isInitialize)
	{
		DEBUG_LOG_WARNING("すでにTerrainSystemは初期化されています\n");
		return false;
	}

	if (!_rootSig || !_pipelineState)
	{
		DEBUG_LOG_ERROR("TerrainSystem初期化に必要なパラメータが不正です\n");
		return false;
	}

	rootSig = _rootSig;
	pipeline = _pipelineState;

	if (!CreateGrid()) return false;

	if (!ringCBV.Setup(sizeof(TerrainCB))) return false;
	isInitialize = true;
	return true;
}

void TerrainSystem::Shutdown()
{
	// 各リソースを空にする
	rootSig.Reset();
	pipeline.Reset();
	indexBuffer = IndexBuffer{};
	vertexBuffer = VertexBuffer{};
	ringCBV.Shutdown();
	terrainContexts.clear();
	isInitialize = false;
 }

void TerrainSystem::RegsterTerrain(Vector3 _position, float _scale, float _tessFactor, float _heightScale, Vector4 _color, TexHandle _heightMap, const Mat4x4& _viewProjectionMatrix)
{
	if (!isInitialize) return;

	// Ring側の上限を超える前に登録を拒否する
	if (terrainContexts.size() >= MAX_CB_PER_FRAME)
	{
		DEBUG_LOG_ERROR("Terrainの登録数がRingConstantBufferの上限に達しました\n");
		return;
	}

	float effectiveHeightScale{ _heightScale }; // 高さのキャッシュ

	GraphicsResourceManager& instance{ GraphicsResourceManager::Instance() };
	TextureData* heightMapData{ instance.Lookup(_heightMap) };
	if (!heightMapData) // heightMapがないとき
	{
		_heightMap = instance.GetDefaultTexture(); // 白テクスチャを入れる
		effectiveHeightScale = 0.0f;
		heightMapData = instance.Lookup(_heightMap); // 白テクスチャを使う
	}

	if (!heightMapData)
	{
		DEBUG_LOG_ERROR("heighMapがデフォルトを含め失敗しました\n");
		return;
	}

	if (_scale <= 0.0f)
	{
		DEBUG_LOG_ERROR("スケールに不正な値が渡されました\n");
		return;
	}

	//XZ方向に拡大
	const Mat4x4 scaleMat{ Mat4x4::MakeScaling(Vector3{_scale, 1.0f, _scale}) };
	const Mat4x4 translationMat{ Mat4x4::MakeTranslation(_position) };
	const Mat4x4 worldMat{ scaleMat * translationMat }; // 行優先なのでS->T
	TerrainCB cb{};
	cb.mvp = worldMat * _viewProjectionMatrix;
	cb.color = _color;
	cb.heightScale = effectiveHeightScale;
	cb.tessFactor = std::clamp(_tessFactor, 1.0f, 64.0f); // HSの分割係数の有効範囲内(1-64)にClamp
	// 同じCBをHSとDSへ渡す
	const D3D12_GPU_VIRTUAL_ADDRESS cbAddress{ ringCBV.Update(&cb, sizeof(TerrainCB)) };
	if (cbAddress <= 0)
	{
		DEBUG_LOG_ERROR("terrainのリングバッファUpdateに失敗しました\n");
		return;
	}

	// 必要な情報を入れる
	TerrainContext terrainContext{};
	terrainContext.gpuVirtualAddress = cbAddress;
	terrainContext.heightMap = _heightMap;
	terrainContexts.push_back(std::move(terrainContext)); // 配列に追加
}

void TerrainSystem::Flush(ID3D12GraphicsCommandList* _cmd)
{
	if (!isInitialize) return;
	GraphicsResourceManager& instance{GraphicsResourceManager::Instance()};
	for (const TerrainContext& context : terrainContexts)
	{
		TextureData* heightMapData{ instance.Lookup(context.heightMap) };

		// 登録後にUnloadされた要求はスキップ
		if (!heightMapData) continue;
		if (context.gpuVirtualAddress == 0) continue;

		// ホットパスのためログは出さず止める
		if (!_cmd || !vertexBuffer.resource || !indexBuffer.resource || !rootSig || !pipeline) return;

		_cmd->SetGraphicsRootSignature(rootSig.Get());
		_cmd->SetPipelineState(pipeline.Get());
		DescriptorManager::Instance().SetDiscriptor(_cmd); // SRVを使うのでDescriptorHeapをセット

		// RootParam[0] : HS b0
		_cmd->SetGraphicsRootConstantBufferView(0, context.gpuVirtualAddress);
		// RootParam[1] : DS b0
		_cmd->SetGraphicsRootConstantBufferView(1, context.gpuVirtualAddress);
		// RootParam[2] : DS t0
		_cmd->SetGraphicsRootDescriptorTable(2, heightMapData->srvHandle.gpu); // 登録時点で安全なハイトマップが保証されている
		_cmd->IASetVertexBuffers(0, 1, &vertexBuffer.vertexView);
		_cmd->IASetIndexBuffer(&indexBuffer.indexView);
		// 3インデックスで1つの三角形パッチとして渡す
		_cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST);
		GraphicsMetrics::RecordDrawCall(GraphicsPass::Terrain);
		_cmd->DrawIndexedInstanced(indexBuffer.indexCount, 1, 0, 0, 0);
	}
}

void TerrainSystem::Reset()
{
	terrainContexts.clear();
	ringCBV.Reset();
}

bool TerrainSystem::CreateGrid()
{
	constexpr UINT GRID_SIZE{ 8 };
	std::vector<TexVertex> vertices{};
	std::vector<uint32_t> indices{};
	vertices.reserve(GRID_SIZE * GRID_SIZE); // 正方形
	indices.reserve((GRID_SIZE - 1) * (GRID_SIZE - 1) * 6);

	// 頂点作成
	for (UINT z = 0; z < GRID_SIZE; z++)
	{
		for (UINT x = 0; x < GRID_SIZE; x++)
		{
			const float u{ static_cast<float>(x) / static_cast<float>(GRID_SIZE - 1) };
			const float v{ static_cast<float>(z) / static_cast<float>(GRID_SIZE - 1) };
			TexVertex vertex{};

			// 1x1のサイズで中心が原点のXZ平面を作る(実際の大きさはWorld行列で変更)
			vertex.position[0] = u - 0.5f;
			vertex.position[1] = 0.0f;
			vertex.position[2] = v - 0.5f;
			vertex.uv[0] = u;
			vertex.uv[1] = v;
			vertices.push_back(vertex);
		}
	}

	// インデックス作成
	for (UINT z = 0; z < GRID_SIZE - 1; z++)
	{
		for (UINT x = 0; x < GRID_SIZE - 1; x++)
		{
			const uint32_t i0{ z * GRID_SIZE + x };
			const uint32_t i1{ i0 + 1 };
			const uint32_t i2{ i0 + GRID_SIZE };
			const uint32_t i3{ i2 + 1 };

			// 1マス目の三角形
			indices.push_back(i0);
			indices.push_back(i2);
			indices.push_back(i1);

			// 2枚目の三角形
			indices.push_back(i1);
			indices.push_back(i2);
			indices.push_back(i3);
		}
	}

	vertexBuffer = GraphicsResourceManager::Instance().CreateVertexBuffer(vertices.data(), static_cast<UINT>(vertices.size() * sizeof(TexVertex)), sizeof(TexVertex));
	indexBuffer = GraphicsResourceManager::Instance().CreateIndexBuffer(indices.data(), static_cast<UINT>(indices.size() * sizeof(uint32_t)), static_cast<UINT>(indices.size()));
	if (!indexBuffer.resource || !vertexBuffer.resource)
	{
		DEBUG_LOG_ERROR("Terrainグリッドの作成に失敗しました\n");
		return false;
	}
	return true;
}
