#include "../External/Common/d3dx12.h"
#include "../External/cgltf.h"
#include "../Window/Window.h"
#include "../Debug/DebugLogs.h"
#include "../Graphics/GraphicsDevice.h"
#include "../Graphics/DescriptorManager.h"
#include "../Graphics/ShaderSystem.h"
#include "../Graphics/GraphicsResourceManager.h"
#include "../Graphics/SpriteBatch.h"
#include "../Graphics/ShapeBatch.h"
#include "../Graphics/RingConstantBuffer.h"
#include "../Graphics/GPUMarker.h"
#include "../Math/TSMath.h"
#include "../Graphics/GraphicsConstant.h"
#include "../Graphics/GraphicsType.h"
#include "GfxInternal.h" // 外部公開しないもの
#include "Gfx.h" // 外部公開するもの

// 無名名前空間で変数を保持する
namespace {
	Window window; // window作成クラス
	RTHandle sceneRenderTarget{}; // シーン全体を描画する内部用RenderTarget
	D3D12_CPU_DESCRIPTOR_HANDLE currentRTV{}; // 現在OMSetRenderTargetsで設定しているRTV
	ShaderSystem shaderSystem; // Shader読み込みなどを管理するファイル
	ConstantBufferData orthConstantBufferData; // 正射影行列用定数バッファのデータメンバ
	RingConstantBuffer mvpRingCBV; // MVP行列用定数バッファのデータメンバ
	RingConstantBuffer materialRingCBV; // material用定数バッファのデータメンバ
	RingConstantBuffer skinningRingCBV; // スキニング行列定数バッファのデータメンバ
	// 毎回内容を更新する定数バッファ
	RingConstantBuffer terrainRingCBV{};
	// 全Terrain描画で共有するグリッド
	VertexBuffer terrainVertexBuffer{};
	IndexBuffer terrainIndexBuffer{};
	Mat4x4 vpMat; // View * Projection
	Mat4x4 mvpMat;
	SpriteBatch fgBatch; // 手前のスプライトバッチ処理
	SpriteBatch bgBatch; // 背景のスプライトバッチ処理
	ShapeBatch shapeBatch; // 基本図形のバッチ処理
	Gfx::BitmapFont defaultFont; // デフォルト用の文字列
	int screenWidth = 0; // 画面の横幅
	int screenHeight = 0; // 画面の縦幅
	bool isSceneRenderTargetActive{ false }; 	// このフレームでシーンRTを描画先として使用できたか
	MaterialHandle currentPostEffectMaterial{}; // 現在画面全体へ適用しているポストエフェクトmaterial(無効ハンドルなら内蔵の素通しPSOを使う)

	constexpr GraphicsPipelineDesc PIPELINE_TABLE[]{
		// 図形塗りつぶし
		{.rootSignatureID = RootSigID::Shape, .pipelineID = PipelineID::ShapeFill,
		  .vsPath = L"../Src/Shaders/ShapeVS.hlsl", .psPath = L"../Src/Shaders/ShapePS.hlsl", 
		  .layout = InputLayout::Shape, .blend = BlendMode::Alpha, .depth = DepthParam::None },
		  // 図形ワイヤー
		{.rootSignatureID = RootSigID::Shape, .pipelineID = PipelineID::ShapeWire,
		  .vsPath = L"../Src/Shaders/ShapeVS.hlsl", .psPath = L"../Src/Shaders/ShapePS.hlsl",
		  .layout = InputLayout::Shape, .blend = BlendMode::Alpha, .depth = DepthParam::None,
		  .topology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE },
		  // 3Dモデル
		{.rootSignatureID = RootSigID::Model, .pipelineID = PipelineID::Model,
		 .vsPath = L"../Src/Shaders/ModelVS.hlsl", .psPath = L"../Src/Shaders/ModelPS.hlsl",
		 .layout = InputLayout::Model, .blend = BlendMode::Opaque, .depth = DepthParam::ReadWrite},
		 // テクスチャ 
		 {.rootSignatureID = RootSigID::Texture, .pipelineID = PipelineID::Sprite,
		  .vsPath = L"../Src/Shaders/TextureVS.hlsl", .psPath = L"../Src/Shaders/TexturePS.hlsl",
		  .layout = InputLayout::Sprite, .blend = BlendMode::Alpha, .depth = DepthParam::None},
		  // Terrain
		{.rootSignatureID = RootSigID::Terrain, .pipelineID = PipelineID::TerrainWire,
		.vsPath = L"../Src/Shaders/TerrainVS.hlsl", .psPath = L"../Src/Shaders/TerrainPS.hlsl",
		.hsPath = L"../Src/Shaders/TerrainHS.hlsl", .dsPath = L"../Src/Shaders/TerrainDS.hlsl",
		.layout = InputLayout::Texture, .blend = BlendMode::Opaque, .depth = DepthParam::ReadWrite, // textureを流用できる
		.topology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH, .fillMode = D3D12_FILL_MODE_WIREFRAME},// hsとdsを使うのでパッチ系のpipelineという大分類にする , 分割された三角形を確認できるようにワイヤー
		// PostEffect
		{.rootSignatureID = RootSigID::PostEffect, .pipelineID = PipelineID::PostEffect,
		 .vsPath = L"../Src/Shaders/PostEffectVS.hlsl", .psPath = L"../Src/Shaders/PostEffectPS.hlsl",
		 .layout = InputLayout::None, .blend = BlendMode::Opaque, // レイアウトはSV_VertexIDから直接作るので頂点入力はない、Blendも完全に画面を置き換えるのでブレンド無し
		 .depth = DepthParam::None, .topology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE} // 2D画像を画面へ貼るだけなので深度は使わない
	};
}

namespace {
	// スキンメッシュ付き
	void DrawSkinnedModel(AnimInstanceData& _anim, Transform _transform)
	{
		GraphicsResourceManager::Instance().UpdateGlobalPose(_anim);

		// skinningRingCBVはMAX_BONE_NUM個分しか確保していないため GPUへ送る前に上限を確認する
		if (_anim.skinningMatrices.size() > MAX_BONE_NUM)
		{
			DEBUG_LOG_ERROR("モデルのボーン数が上限を超えています ""boneCount:{} max:{}\n", _anim.skinningMatrices.size(), MAX_BONE_NUM);
			return;
		}

		ModelData* model{ GraphicsResourceManager::Instance().Lookup(_anim.handle) }; // ハンドル分解
		if (!model) return;

		auto cmd{ GraphicsDevice::Instance().GetCommandList() };
		Mat4x4 worldMat{ _transform.GetWorldMatrix() }; // ワールド行列の取得
		mvpMat = worldMat * vpMat;

		cmd->SetGraphicsRootSignature(shaderSystem.GetRootSignature(RootSigID::Model));
		cmd->SetPipelineState(shaderSystem.GetPipeline(PipelineID::Model));

		DescriptorManager::Instance().SetDiscriptor(cmd);
		const D3D12_GPU_VIRTUAL_ADDRESS mvpUpdate{ mvpRingCBV.Update(&mvpMat, sizeof(Mat4x4)) };
		const D3D12_GPU_VIRTUAL_ADDRESS skinningUpdate{ skinningRingCBV.Update(_anim.skinningMatrices.data(), sizeof(Mat4x4) * static_cast<UINT>(_anim.skinningMatrices.size())) };
		if ((mvpUpdate <= 0) || (skinningUpdate <= 0))
		{
			DEBUG_LOG_ERROR("mvpもしくはスキンのUpdateで失敗しました\n");
			return;
		}
		cmd->SetGraphicsRootConstantBufferView(0, mvpUpdate); // MVP更新
		cmd->SetGraphicsRootConstantBufferView(2, skinningUpdate); // ボーンを更新
		cmd->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		// サブメッシュ分回す
		for (const SubMesh& sub : model->subMeshes)
		{
			// material類の更新
			MaterialCB matCB{};
			matCB.baseColorFactor = sub.material.baseColorFactor;
			matCB.metallic = sub.material.metallic;
			matCB.roughness = sub.material.roughness;
			matCB.emissiveFactor = sub.material.emissiveFactor;
			const D3D12_GPU_VIRTUAL_ADDRESS materialUpdate{ materialRingCBV.Update(&matCB, sizeof(MaterialCB)) };
			if (materialUpdate <= 0)
			{
				DEBUG_LOG_ERROR("マテリアルringbufferのUpateで失敗しました\n");
				return; // materialのUpdateで失敗したらモデルをあきらめる
			}
			cmd->SetGraphicsRootConstantBufferView(1, materialUpdate);

			TextureData* tex{ GraphicsResourceManager::Instance().Lookup(sub.material.textures[MaterialTex::BaseColor]) };
			if (tex)
			{
				cmd->SetGraphicsRootDescriptorTable(3, tex->srvHandle.gpu);
			}
			else
			{
				DEBUG_LOG_ERROR("モデルのLookUpに失敗しました\n");
				TextureData* error{ GraphicsResourceManager::Instance().Lookup(GraphicsResourceManager::Instance().GetErrorTexture()) }; // エラーハンドルを分解
				if (!error)
				{
					DEBUG_LOG_ERROR("モデルLookup失敗時にエラー用テクスチャのLookUpに失敗しました\n");
					return;
				}
				cmd->SetGraphicsRootDescriptorTable(3, error->srvHandle.gpu);
			}

			cmd->IASetVertexBuffers(0, 1, &sub.vertexBuffer.vertexView);
			cmd->IASetIndexBuffer(&sub.indexBuffer.indexView);
			cmd->DrawIndexedInstanced(sub.indexBuffer.indexCount, 1, 0, 0, 0);
		}
	}
	// スキンメッシュなし
	void DrawStaticModel(ModelHandle _model, const Transform _transform)
	{
		ModelData* model{ GraphicsResourceManager::Instance().Lookup(_model) };
		if (!model) return; // 無効ハンドルガード
		auto cmd{ GraphicsDevice::Instance().GetCommandList() }; // コマンドリストのキャッシュ
		Mat4x4 worldMat{ _transform.GetWorldMatrix() };
		mvpMat = worldMat * vpMat;

		// パイプライン設定
		cmd->SetGraphicsRootSignature(shaderSystem.GetRootSignature(RootSigID::Model));
		cmd->SetPipelineState(shaderSystem.GetPipeline(PipelineID::Model));

		DescriptorManager::Instance().SetDiscriptor(cmd); // Flushと同じ考え方

		const D3D12_GPU_VIRTUAL_ADDRESS mvpUpdate{ mvpRingCBV.Update(&mvpMat, sizeof(Mat4x4)) };
		const D3D12_GPU_VIRTUAL_ADDRESS skinningUpdate{ skinningRingCBV.Update(&Mat4x4::Identity, sizeof(Mat4x4)) };
		if ((mvpUpdate <= 0) || (skinningUpdate <= 0))
		{
			DEBUG_LOG_ERROR("mvpもしくはスキンのUpdateで失敗しました\n");
			return;
		}

		cmd->SetGraphicsRootConstantBufferView(0, mvpUpdate);
		// 静的描画の場合は単位行列を送る
		cmd->SetGraphicsRootConstantBufferView(2, skinningUpdate);
		cmd->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);



		// submeshループ
		for (const SubMesh& sub : model->subMeshes)
		{
			// material値をCBにつめる
			MaterialCB matCB{};
			matCB.baseColorFactor = sub.material.baseColorFactor;
			matCB.metallic = sub.material.metallic;
			matCB.roughness = sub.material.roughness;
			matCB.emissiveFactor = sub.material.emissiveFactor;
			const D3D12_GPU_VIRTUAL_ADDRESS materialUpdate{ materialRingCBV.Update(&matCB, sizeof(MaterialCB)) };
			if (materialUpdate <= 0)
			{
				DEBUG_LOG_ERROR("マテリアルringbufferのUpateで失敗しました\n");
				return; // materialのUpdateで失敗したらモデルをあきらめる
			}
			// Ringで送ってb1にバインドする
			cmd->SetGraphicsRootConstantBufferView(1, materialUpdate);

			// テクスチャをバインド
			TextureData* tex{ GraphicsResourceManager::Instance().Lookup(sub.material.textures[MaterialTex::BaseColor]) };
			if (tex)
			{
				cmd->SetGraphicsRootDescriptorTable(3, tex->srvHandle.gpu);
			}
			else
			{
				DEBUG_LOG_ERROR("モデルのLookUpに失敗しました\n");
				TextureData* error{ GraphicsResourceManager::Instance().Lookup(GraphicsResourceManager::Instance().GetErrorTexture()) }; // エラーハンドルを分解
				if (!error)
				{
					DEBUG_LOG_ERROR("モデルLookup失敗時にエラー用テクスチャのLookUpに失敗しました\n");
					return;
				}
				cmd->SetGraphicsRootDescriptorTable(3, error->srvHandle.gpu);
			}

			// 頂点インデックスをバインド
			cmd->IASetVertexBuffers(0, 1, &sub.vertexBuffer.vertexView);
			cmd->IASetIndexBuffer(&sub.indexBuffer.indexView);

			cmd->DrawIndexedInstanced(sub.indexBuffer.indexCount, 1, 0, 0, 0);
		}
	}

	// Terrainのリソースを初期化する
	bool InitializeTerrainResources()
	{
		constexpr UINT GRID_SIZE{ 8 };
		std::vector<TexVertex> vertices{};
		std::vector<uint32_t> indices{};
		vertices.reserve(GRID_SIZE * GRID_SIZE); // 正方形
		indices.reserve((GRID_SIZE - 1) * (GRID_SIZE - 1) * 6);

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

		terrainVertexBuffer = GraphicsResourceManager::Instance().CreateVertexBuffer(vertices.data(), static_cast<UINT>(vertices.size() * sizeof(TexVertex)), sizeof(TexVertex));
		terrainIndexBuffer = GraphicsResourceManager::Instance().CreateIndexBuffer(indices.data(), static_cast<UINT>(indices.size() * sizeof(uint32_t)), static_cast<UINT>(indices.size()));
		if (!terrainIndexBuffer.resource || !terrainVertexBuffer.resource)
		{
			DEBUG_LOG_ERROR("Terrainグリッドの作成に失敗しました\n");
			return false;
		}
		terrainRingCBV.Initialize(sizeof(TerrainCB));
		return true;
	}

	// Terrain描画の内部処理
	void DrawTerrainInternal(Vector3 _position, float _scale, float _tessFactor, float _heightScale, Vector4 _color, TexHandle _heightMap)
	{
		if (_scale <= 0.0f)
		{
			DEBUG_LOG_WARNING("Terrainのスケールは0より大きくしてください\n");
			return;
		}
		ID3D12GraphicsCommandList* cmd{ GraphicsDevice::Instance().GetCommandList() };
		if (!cmd || !terrainVertexBuffer.resource || !terrainIndexBuffer.resource) return;

		// 指定されたHeightMapの実データ取得
		TextureData* heightMap{ GraphicsResourceManager::Instance().Lookup(_heightMap) };
		float effectiveHeightScale{ _heightScale }; // 高さのキャッシュ
		if (!heightMap) // heightMapがないとき
		{
			const TexHandle fallback{ GraphicsResourceManager::Instance().GetDefaultTexture() };
			heightMap = GraphicsResourceManager::Instance().Lookup(fallback); // 白テクスチャを使う
			effectiveHeightScale = 0.0f; // ハイトマップがないときは高さ0にする
		}
		if (!heightMap)
		{
			DEBUG_LOG_ERROR("heighMapがデフォルトを含め失敗しました\n");
			return;
		}

		// Terrain用RootSigとPSO
		cmd->SetGraphicsRootSignature(shaderSystem.GetRootSignature(RootSigID::Terrain));
		cmd->SetPipelineState(shaderSystem.GetPipeline(PipelineID::TerrainWire));
		DescriptorManager::Instance().SetDiscriptor(cmd); // SRVを使うのでDescriptorHeapをセット
		//XZ方向に拡大
		const Mat4x4 scaleMat{ Mat4x4::MakeScaling(Vector3{_scale, 1.0f, _scale}) };
		const Mat4x4 translationMat{ Mat4x4::MakeTranslation(_position) };
		const Mat4x4 worldMat{ scaleMat * translationMat }; // 行優先なのでS->T
		TerrainCB cb{};
		cb.mvp = worldMat * vpMat;
		cb.color = _color;
		cb.heightScale = effectiveHeightScale;
		cb.tessFactor = std::clamp(_tessFactor, 1.0f, 64.0f); // HSの分割係数の有効範囲内(1-64)にClamp
		// 同じCBをHSとDSへ渡す
		const D3D12_GPU_VIRTUAL_ADDRESS cbAddress{ terrainRingCBV.Update(&cb, sizeof(TerrainCB)) };
		if (cbAddress <= 0)
		{
			DEBUG_LOG_ERROR("terrainのリングバッファUpdateに失敗しました\n");
			return;
		}
		// RootParam[0] : HS b0
		cmd->SetGraphicsRootConstantBufferView(0, cbAddress);
		// RootParam[1] : DS b0
		cmd->SetGraphicsRootConstantBufferView(1, cbAddress);
		// RootParam[2] : DS t0
		cmd->SetGraphicsRootDescriptorTable(2, heightMap->srvHandle.gpu);
		cmd->IASetVertexBuffers(0, 1, &terrainVertexBuffer.vertexView);
		cmd->IASetIndexBuffer(&terrainIndexBuffer.indexView);
		// 3インデックスで1つの三角形パッチとして渡す
		cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST);
		cmd->DrawIndexedInstanced(terrainIndexBuffer.indexCount, 1, 0, 0, 0);
	}

	void ShutdownGfxOwnedResources()
	{
		// 仮で作っているTerrainのVB.IBを解放する(これは一時的な物なので3Dの基本図形描画時になくなる予定)
		terrainIndexBuffer = IndexBuffer{};
		terrainVertexBuffer = VertexBuffer{};
		// RingConstantBufferの解放
		mvpRingCBV.Shutdown();
		materialRingCBV.Shutdown();
		skinningRingCBV.Shutdown();
		terrainRingCBV.Shutdown();
		// Batchが所有するVB,IBを解放
		fgBatch.Shutdown();
		bgBatch.Shutdown();
		shapeBatch.Shutdown();

		// 正射影CBを解放する
		if (orthConstantBufferData.cbvHandle.IsValid())
		{
			DescriptorManager::Instance().Free(HeapType::CBV_SRV_UAV,orthConstantBufferData.cbvHandle);
		}

		orthConstantBufferData = ConstantBufferData{};

		// GraphicsResourceManagerのShutdownで生存中のRTが回収されるが明示しておく
		if (sceneRenderTarget.IsValid())
		{
			GraphicsResourceManager::Instance().Unload(sceneRenderTarget);
			sceneRenderTarget = RTHandle{};
		}
		currentRTV = {};
		currentPostEffectMaterial = {};
	}
}

// 初期化処理(これを呼ぶだけで初期化処理が済むようにする)
bool GfxInternal::Initialize(const wchar_t* _title, int _width, int _height)
{
	HRESULT result{};

	screenWidth = _width;
	screenHeight = _height;
	// 前回の初期化状態を引き継がない
	currentPostEffectMaterial = {};

	window.SetWindowName(_title); // 名前設定
	window.GenerateWindow(); // ウィンドウを作成
	if (!window.GetHWND())
	{
		DEBUG_LOG_ERROR("ウィンドウ作成に失敗しました\n");
		return false; // ウィンドウ作成失敗ならfalse
	}

	GraphicsDevice::Instance().Initialize(window.GetHWND(), _width, _height); // デバイスの初期化
	if (!GraphicsDevice::Instance().GetDevice())
	{
		DEBUG_LOG_ERROR("デバイスの読み込みに失敗しました\n");
		return false; // デバイス読み込み失敗したらfalse
	}

	DescriptorManager::Instance().Initialize(GraphicsDevice::Instance().GetDevice()); // ディスクリプタマネージャーをデバイスを使って初期化

	shaderSystem.Setup(GraphicsDevice::Instance().GetDevice()); // ShaderSystemの初期化

	// 汎用するRootSignatureの作成
	const std::vector<RootSignatureDesc> rootSignatureDescs{ shaderSystem.MakeRootSignatureDescs() };
	for (const RootSignatureDesc& desc : rootSignatureDescs)
	{
		if (!shaderSystem.CreateRootSignature(desc))
		{
			DEBUG_LOG_ERROR("RootSignatureの作成に失敗しました\n");
			return false;
		}
	}

	// rootSignatureをつかってPSOを作成
	for (const GraphicsPipelineDesc& desc : PIPELINE_TABLE)
	{
		if (!shaderSystem.CreateGraphicsPipeline(desc))
		{
			DEBUG_LOG_ERROR("GraphicsPipelineの作成に失敗しました\n");
			return false;
		}
	}

	GraphicsResourceManager::Instance().Initialize(GraphicsDevice::Instance().GetDevice()); // リソース管理ファイルの初期化
	
	// 画面と同じサイズの内部描画先を作成
	sceneRenderTarget = GraphicsResourceManager::Instance().CreateRenderTarget(static_cast<UINT>(_width), static_cast<UINT>(_height));
	if (!sceneRenderTarget.IsValid())
	{
		DEBUG_LOG_ERROR("シーン描画用RenderTargetの作成に失敗しました\n");
		return false;
	}
	// Lookup確認
	RenderTargetData* sceneRT{ GraphicsResourceManager::Instance().Lookup(sceneRenderTarget) };
	if (!sceneRT)
	{
		DEBUG_LOG_ERROR("シーン描画用RenderTargetの取得に失敗しました\n");
		return false;
	}
	currentRTV = sceneRT->rtvHandle.cpu; // 初期状態として内部RTを現在の描画先とする

	// グリッドとCBの作成
	if (!InitializeTerrainResources()) return false;

	// ピクセル座標からNDC座標へ変換
	Mat4x4 orthMat{ Mat4x4::MakeOrthGraphic(static_cast<float>(_width), static_cast<float>(_height)) }; // 変換行列の作成
	orthConstantBufferData = GraphicsResourceManager::Instance().CreateConstantBuffer(&orthMat, sizeof(Mat4x4));

	// 透視投影行列の作成(一旦キューブが描画できるのを確認するためにハードコーディング)
	vpMat = Mat4x4::MakeLookAt({ 0.0f, 3.0f, -3.0f }, { 0.0f, 1.0f, 0.0f }, Vector3::Up) * Mat4x4::MakePerspective(60.0f * Math::DEG_TO_RAD, static_cast<float>(screenWidth) / static_cast<float>(screenHeight), 0.1f, 100.0f);
	mvpRingCBV.Initialize(sizeof(Mat4x4)); // リングバッファ初期化
	materialRingCBV.Initialize(sizeof(MaterialCB));  // materialのリング定数バッファを初期化
	skinningRingCBV.Initialize(sizeof(Mat4x4) * MAX_BONE_NUM); // ボーン用の定数バッファを更新

	// スプライトバッチ処理初期化
	fgBatch.Initialize(shaderSystem.GetRootSignature(RootSigID::Texture), shaderSystem.GetPipeline(PipelineID::Sprite), orthConstantBufferData.resource.Get());
	bgBatch.Initialize(shaderSystem.GetRootSignature(RootSigID::Texture), shaderSystem.GetPipeline(PipelineID::Sprite), orthConstantBufferData.resource.Get());
	shapeBatch.Initialize(shaderSystem.GetRootSignature(RootSigID::Shape),shaderSystem.GetPipeline(PipelineID::ShapeFill), shaderSystem.GetPipeline(PipelineID::ShapeWire), orthConstantBufferData.resource.Get());

	// 文字列構造体初期化
	defaultFont.texture = Gfx::LoadTexture("../Src/External/Res/DejaVu Sans Mono.png"); // デフォルトフォント
	if (!defaultFont.texture.IsValid())
	{
		DEBUG_LOG_ERROR("無効なハンドルが渡されました\n");
	}
	defaultFont.texWidth = 256; // 全体横幅
	defaultFont.texHeight = 256; // 全体縦幅
	defaultFont.cellWidth = 16; // セル幅
	defaultFont.cellHeight = 16; // セル高さ
	defaultFont.cols = 16; // 行の要素数
	defaultFont.firstCode = 0; // CP437配列なので0
	return true;
}

// フレーム開始処理
void GfxInternal::BeginFrame()
{
	GraphicsDevice::Instance().BeginFrame(); // フレームの最初の処理

	// GPUが使用し終えた遅延開放リソースを回収する
	GraphicsResourceManager::Instance().CollectDeferredReleases(GraphicsDevice::Instance().GetCompletedFenceValue());

	// batch処理のカウンターリセット
	bgBatch.Reset();
	fgBatch.Reset();
	shapeBatch.Reset();
	// 定数バッファのカウンターリセット
	mvpRingCBV.Reset();
	materialRingCBV.Reset();
	skinningRingCBV.Reset();
	terrainRingCBV.Reset();

	auto cmdList{ GraphicsDevice::Instance().GetCommandList() }; // コマンドリスト
	auto dsv{ GraphicsDevice::Instance().GetDSV() };
	RenderTargetData* sceneRT{ GraphicsResourceManager::Instance().Lookup(sceneRenderTarget) }; // 内部ハンドルを分解した時のデータ
	isSceneRenderTargetActive = false; // BeginFrameごとに使用状態を決め直す
	if (sceneRT)
	{
		//通常経路としてシーンRTへ描画する
		currentRTV = sceneRT->rtvHandle.cpu;
		isSceneRenderTargetActive = true;
	}
	else
	{
		DEBUG_LOG_ERROR("シーンRTを取得できないのでバックバッファへ直接描画します\n");
		currentRTV = GraphicsDevice::Instance().GetCurrentRTV();
	}

	// 深度バッファとステンシルバッファをクリアする
	cmdList->ClearDepthStencilView(
		dsv, // DSVハンドル
		D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, // 深度バッファとステンシルバッファ両方クリア
		1.0f, // 深度クリア値(最も遠くから)
		0, // 全体クリア
		0, // 全体クリア
		nullptr // 全体クリア
	);

	// レンダーターゲット設定
	cmdList->OMSetRenderTargets(1, &currentRTV, false, &dsv);

	// ビューポート
	D3D12_VIEWPORT viewPort{};
	viewPort.TopLeftX = 0.0f;
	viewPort.TopLeftY = 0.0f;
	viewPort.Width = static_cast<float>(screenWidth);
	viewPort.Height = static_cast<float>(screenHeight);
	viewPort.MinDepth = 0.0f;
	viewPort.MaxDepth = 1.0f;
	cmdList->RSSetViewports(1, &viewPort);

	// シザー矩形
	D3D12_RECT scissorRect{};
	scissorRect.left = 0;
	scissorRect.top = 0;
	scissorRect.right = screenWidth;
	scissorRect.bottom = screenHeight;
	cmdList->RSSetScissorRects(1, &scissorRect);

}

// フレーム終了処理
void GfxInternal::EndFrame()
{
	// Spritebatch描画
	{
		GPU_MARKER("backGround");
		bgBatch.Flush();
	}

	{
		GPU_MARKER("foreGround");
		fgBatch.Flush();
	}
	// ShapeBatch描画
	{
		GPU_MARKER("ShapeDraw");
		shapeBatch.Flush();
	}

	auto* cmd{ GraphicsDevice::Instance().GetCommandList() };

	// このフレームでオフスクリーンが使われていたら
	if (isSceneRenderTargetActive)
	{
		RenderTargetData* sceneRT{ GraphicsResourceManager::Instance().Lookup(sceneRenderTarget) };
		if (sceneRT)
		{
			// バリアを使ってシーンRTを書き込み先からシェーダーで読む画像へ遷移させる
			D3D12_RESOURCE_BARRIER toShaderResource{ CD3DX12_RESOURCE_BARRIER::Transition(sceneRT->resource.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE) };
			cmd->ResourceBarrier(1, &toShaderResource);

			// 描画先をシーンRTからバックバッファへ変更する
			currentRTV = GraphicsDevice::Instance().GetCurrentRTV();
			// このパスでは深度を使わないのでDSVにはnullを渡す
			cmd->OMSetRenderTargets(1, &currentRTV, false, nullptr);
			// シーンRTをフルスクリーン三角形として描画する
			cmd->SetGraphicsRootSignature(shaderSystem.GetRootSignature(RootSigID::PostEffect));
			// 最初は内蔵のPSOを選ぶ
			ID3D12PipelineState* postEffectPipeline{ shaderSystem.GetPipeline(PipelineID::PostEffect) };
			// 外部materialが設定されている場合は台帳から取得
			if (currentPostEffectMaterial.IsValid())
			{
				MaterialData* material{ GraphicsResourceManager::Instance().Lookup(currentPostEffectMaterial) };
				// 内部要素をチェックして全て通っていたらPSO差し替え
				if (material && material->usage == ShaderUsage::PostEffect && material->pipelineState)
				{
					postEffectPipeline = material->pipelineState.Get();
				}
				else
				{
					// Unloadで無効になっていた場合内蔵PSOで描画して次回以降素通しに戻す
					currentPostEffectMaterial = {};
				}
			}
			cmd->SetPipelineState(postEffectPipeline);
			// SRVヒープをコマンドリストへ設定
			DescriptorManager::Instance().SetDiscriptor(cmd);
			// RootSignatureの0番へシーンRTのSRVを渡す
			cmd->SetGraphicsRootDescriptorTable(0, sceneRT->srvHandle.gpu);
			cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			cmd->DrawInstanced(3, 1, 0, 0); // 頂点バッファを使わずにSV_VertexIDの0, 1, 2を発生させる

			// 次フレームで再びシーンRTへ描けるようにする
			D3D12_RESOURCE_BARRIER toRenderTarget{ CD3DX12_RESOURCE_BARRIER::Transition(sceneRT->resource.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET) };
			cmd->ResourceBarrier(1, &toRenderTarget);
		}
		else
		{
			// ポストエフェクトを断念する
			DEBUG_LOG_ERROR("EndFrameでシーンRTを取得できないためポストエフェクトをスキップします\n");
		}
	}
	GraphicsDevice::Instance().EndFrame(); // フレームの最後の処理
	// このフレーム中にUnloadされたリソースに対して今回のSignalしたFence値を割り当てる
	GraphicsResourceManager::Instance().CommitPendingRelease(GraphicsDevice::Instance().GetLastSubmittedFenceValue());
	isSceneRenderTargetActive = false;
}

// 終了処理
void GfxInternal::Finish()
{

	GraphicsDevice& graphicsDevice{ GraphicsDevice::Instance() };

#ifdef _DEBUG
	// Wait前の状態を確認する
	// submittedよりcompletedが小さければ、終了処理に入った時点でGPUはまだ動いている
	const UINT64 submittedBefore{ graphicsDevice.GetLastSubmittedFenceValue() };

	const UINT64 completedBefore{ graphicsDevice.GetCompletedFenceValue() };

	DEBUG_LOG("[FenceSensor BeforeWait] submitted={} completed={} inFlight={}", submittedBefore, completedBefore, completedBefore < submittedBefore);
#endif

	bool result{ GraphicsDevice::Instance().WaitForGPU() }; // GPUの待機をしてから各終了処理を行う
	if (!result)
	{
		DEBUG_LOG_ERROR("Finish関数にてGPU待機処理に失敗しました\n");
		return;
	}

#ifdef _DEBUG
	// WaitForGPU内で新しいフェンス値をSignalしているため、
	// Wait前の値を使い回さず、両方とも改めて取得する
	const UINT64 submittedAfter{ graphicsDevice.GetLastSubmittedFenceValue() };

	const UINT64 completedAfter{ graphicsDevice.GetCompletedFenceValue() };

	DEBUG_LOG("[FenceSensor AfterWait] submitted={} completed={} inFlight={}", submittedAfter, completedAfter, completedAfter < submittedAfter);
#endif

	ShutdownGfxOwnedResources(); // Gfxが所有するリソースの削除
	GraphicsResourceManager::Instance().Shutdown(); // 残っている全てのGraphicsResource解放
	shaderSystem.Shutdown(); // PS・RootSignature解放
	DescriptorManager::Instance().Shutdown(); // 全てのDescriptorが不要になった後に解放


#ifdef _DEBUG

	ID3D12Device* device{ graphicsDevice.GetDevice() };
	if (device != nullptr)
	{
		const HRESULT reason = device->GetDeviceRemovedReason();

		DEBUG_LOG("[Sensor1] GetDeviceRemovedReason = 0x{:08X}", static_cast<unsigned int>(reason));
	}
#endif

	GraphicsDevice::Instance().Shutdown(); // Deviceの解放
}

// 描画先をクリアする(色指定可能)
void Gfx::ClearScreen(float _r, float _g, float _b, float _a)
{
	float windowColor[]{ _r, _g, _b, _a };
	if (currentRTV.ptr == 0)
	{
		DEBUG_LOG_ERROR("現在の描画先が設定されていません\n");
		return;
	}
	GraphicsDevice::Instance().GetCommandList()->ClearRenderTargetView(currentRTV, windowColor, 0, nullptr); // コマンドリストを取得しそこから現在書き込んでいるRTVにの色を任意色でクリアする
}

// 画像読み込み
TexHandle Gfx::LoadTexture(const char* _filePath, TextureUsage _usage)
{
	bool isData{ false };
	if (_usage == TextureUsage::Color)
	{
		isData = false;
	}
	else
	{
		isData = true;
	}
	return GraphicsResourceManager::Instance().LoadTexture(_filePath, isData);
}

// モデル読み込み
ModelHandle Gfx::LoadModel(const char* _filePath)
{
	return GraphicsResourceManager::Instance().LoadModel(_filePath);
}

ShaderHandle Gfx::LoadShader(const wchar_t* _filePath, ShaderUsage _usage, ShaderStage _stage)
{
	// nullptrや空文字をコンパイラへ渡さない
	if (!_filePath || _filePath[0] == L'\0')
	{
		DEBUG_LOG_ERROR("Shaderのファイルパスが空です\n");
		return  ShaderHandle{};
	}

	switch (_usage)
	{
	case ShaderUsage::PostEffect:
	case ShaderUsage::Sprite:
		// この二つは現状対応しているのでbreak
		break;
	case ShaderUsage::Model:
		DEBUG_LOG_ERROR("Model用の外部Shaderはまだ対応していません\n");
		return ShaderHandle{};
	default:
		DEBUG_LOG_ERROR("不明なShaderUsageが指定されました\n");
		return ShaderHandle{};
	}

	const char* target{ nullptr };
	switch (_stage)
	{
	case ShaderStage::Vertex:
		target = "vs_5_0";
		break;
	case ShaderStage::Pixel:
		target = "ps_5_0";
		break;
	case ShaderStage::Hull:
		target = "hs_5_0";
		break;
	case ShaderStage::Domain:
		target = "ds_5_0";
		break;
	case ShaderStage::Geometry:
		target = "gs_5_0";
		break;
	case ShaderStage::Compute:
		target = "cs_5_0";
		break;
	default:
		DEBUG_LOG_ERROR("不正なShaderカテゴリが渡されました\n");
		target = nullptr;
		break;
	}

	if (!target)
	{
		return ShaderHandle{};
	}

	ComPtr<ID3DBlob> shaderBlob{ shaderSystem.Compile(_filePath, "main", target) }; // Shaderに対応するtargetでコンパイルする
	if (!shaderBlob)
	{
		DEBUG_LOG_ERROR("Shaderの読み込みに失敗しました\n");
		return ShaderHandle{};
	}

	// コンパイル済みBlobの所有権をShader台帳へ移す
	return GraphicsResourceManager::Instance().RegisterShader(_usage, _stage ,std::move(shaderBlob));
}

MaterialHandle Gfx::CreateMaterial(ShaderHandle _pixelShader)
{
	// 内蔵VSを使うのでvertexは空を渡す
	return CreateMaterial(ShaderHandle{}, _pixelShader);
}

MaterialHandle Gfx::CreateMaterial(ShaderHandle _vertexShader, ShaderHandle _pixelShader)
{
	GraphicsResourceManager& resourceManager{ GraphicsResourceManager::Instance() };
	ShaderData* pixelData{ resourceManager.Lookup(_pixelShader) };
	if (!pixelData)
	{
		DEBUG_LOG_ERROR("PixelShaderHandleが無効です\n");
		return MaterialHandle{};
	}
	if (pixelData->stage != ShaderStage::Pixel)
	{
		DEBUG_LOG_ERROR("PixelShaderの場所にPixel以外のshaderが渡されました\n");
		return MaterialHandle{};
	}

	ID3DBlob* vertexBlob{ nullptr };
	// VertexShaderが指定されている場合
	if (_vertexShader.IsValid())
	{
		ShaderData* vertexData{ resourceManager.Lookup(_vertexShader)};
		if (!vertexData)
		{
			DEBUG_LOG_ERROR("VertexShaderHandleが無効です\n");
			return MaterialHandle{};
		}
		if (vertexData->stage != ShaderStage::Vertex)
		{
			DEBUG_LOG_ERROR("VertexShaderの場所にVertex以外のshaderが渡されました\n");
			return MaterialHandle{};
		}
		// 使用用途を一致させる
		if (vertexData->usage != pixelData->usage)
		{
			DEBUG_LOG_ERROR("PixelとVertexの使用用途が一致していません\n");
			return MaterialHandle{};
		}
		vertexBlob = vertexData->blob.Get();
	}

	ComPtr<ID3D12PipelineState> pipeline{ shaderSystem.CreateMaterialPipeline(pixelData->usage, vertexBlob, pixelData->blob.Get()) };
	if (!pipeline)
	{
		DEBUG_LOG_ERROR("Material用PSOの作成に失敗しました\n");
		return MaterialHandle{};
	}

	return resourceManager.RegisterMaterial(_pixelShader, std::move(pipeline));
}

void Gfx::DrawBox(Vector2 _leftTop, Vector2 _rightBottom, float _radRotation, Vector4 _color, bool _isWireframe)
{
	shapeBatch.RegisterBox(_leftTop, _rightBottom, _radRotation, _color, _isWireframe);
}

void Gfx::DrawCircle(Vector2 _center, float _radius, Vector4 _color, bool _isWireframe)
{
	shapeBatch.RegisterCircle(_center, _radius, _color, _isWireframe);
}

void Gfx::DrawCapsule(Vector2 _startPos, Vector2 _endPos, float _radius, Vector4 _color, bool _isWireframe)
{
	shapeBatch.RegisterCapsule(_startPos, _endPos, _radius, _color, _isWireframe);
}

void Gfx::DrawLine(Vector2 _startPos, Vector2 _endPos, Vector4 _color)
{
	shapeBatch.RegisterLine(_startPos, _endPos, _color);
}

// 文字列描画(デフォルトフォント)
void Gfx::DrawString(const char* _string, Vector2 _position, float _scale, Vector4 _color, LenderLayer _layer)
{
	DrawString(defaultFont, _string, _position, _scale, _color, _layer);
}

// 文字列描画(フォント設定用)
void Gfx::DrawString(const BitmapFont& _font, const char* _string, Vector2 _position, float _scale, Vector4 _color, LenderLayer _layer)
{
	// セルの最終的な大きさ
	Vector2 glyphSize{ _font.cellWidth * _scale, _font.cellHeight * _scale };

	Vector2 cursor{ _position }; // 文字を書く位置
	const float startX{ _position.x }; // 改行で戻る左端
	const int rows{ _font.texHeight / _font.cellHeight }; // 縦のセル数
	const int totalCells{ _font.cols * rows };

	for (const char* p{ _string }; *p != '\0'; ++p)
	{
		// 符号付だと128以上が負になるので符号なしで
		const unsigned char c{ static_cast<unsigned char>(*p) };

		// 改行処理
		if (c == '\n')
		{
			cursor.x = startX;
			cursor.y += glyphSize.y;
			continue;
		}

		const int index{ static_cast<int>(c) - _font.firstCode }; // コード->セル番号
		if (index < 0 || index >= totalCells)
		{
			// 範囲外ならスキップ
			continue;
		}

		const int col{ index % _font.cols }; // 横位置
		const int row{ index / _font.cols }; // 縦位置

		// ピクセル矩形をtexサイズにしてUVに投げる
		Vector2 uvMin{ (col * _font.cellWidth) / static_cast<float>(_font.texWidth), (row * _font.cellHeight) / static_cast<float>(_font.texHeight) };
		Vector2 uvMax{ ((col + 1) * _font.cellWidth) / static_cast<float>(_font.texWidth), ((row + 1) * _font.cellHeight) / static_cast<float>(_font.texHeight) };


		DrawSprite(_font.texture, cursor, glyphSize, 0.0f, _color, uvMin, uvMax, _layer);

		cursor.x += glyphSize.x; // 書いた分右へ
	}
}

// 画像登録
void Gfx::DrawSprite(TexHandle _texture, Vector2 _position, Vector2 _size, float _radRotation, Vector4 _color, Vector2 _uvMin, Vector2 _uvMax, LenderLayer _layer)
{
	// 通常は空のMaterialHandleを渡す
	DrawSprite(_texture, _position, _size, MaterialHandle{}, _radRotation, _color, _uvMin, _uvMax, _layer);
}

void Gfx::DrawSprite(TexHandle _texture, Vector2 _position, Vector2 _size, MaterialHandle _material, float _radRotation, Vector4 _color, Vector2 _uvMin, Vector2 _uvMax, LenderLayer _layer)
{
	ID3D12PipelineState* usePipeline{ shaderSystem.GetPipeline(PipelineID::Sprite) }; // 最初は内蔵SpritePSO
	// 外部materialが指定されている場合
	if (_material.IsValid())
	{
		MaterialData* material{ GraphicsResourceManager::Instance().Lookup(_material) };
		if (!material)
		{
			// このSpriteだけ内蔵PSOへフォールバックされる
		}
		else if (material->usage != ShaderUsage::Sprite)
		{
			DEBUG_LOG_ERROR("Sprite描画にSprite以外のmaterialが渡されました\n");
		}
		else if (!material->pipelineState)
		{
			DEBUG_LOG_ERROR("Sprite用MaterialにPSOがありません\n");
		}
		else
		{
			// 有効なSpriteMaterialなら外部PSOへ差し替える
			usePipeline = material->pipelineState.Get();
		}
	}

	switch (_layer)
	{
	case LenderLayer::BackGround:
		bgBatch.RegisterSprite(_texture, usePipeline,_position, _size, _radRotation, _color, _uvMin, _uvMax);
		break;
	case LenderLayer::ForeGround:
		fgBatch.RegisterSprite(_texture, usePipeline,  _position, _size, _radRotation, _color, _uvMin, _uvMax);
		break;
	default:
		break;
	}
}

void Gfx::DrawModel(ModelHandle _model, Transform _transform, AnimInstanceData* _animData)
{
	{
		// マクロがスコープを抜けるとEndEventするので囲う
		GPU_MARKER("backGround");
		bgBatch.Flush(); // 背景の上に来るように3D描画前には背景batchをFlushする
	}

	// モデルの状況によって分ける
	if (_animData)
	{
		DrawSkinnedModel(*_animData, _transform);
	}
	else
	{
		DrawStaticModel(_model, _transform);
	}

}

void Gfx::DrawTerrain(Vector3 _position, float _scale, float _tessFactor, float _heightScale, Vector4 _color, TexHandle _heightMap)
{
	{
		// マクロがスコープを抜けるとEndEventするので囲う
		GPU_MARKER("backGround");
		bgBatch.Flush(); // 背景の上に来るように3D描画前には背景batchをFlushする
	}

	DrawTerrainInternal(_position, _scale, _tessFactor, _heightScale, _color, _heightMap);
}

void Gfx::SetBaseColor(ModelHandle _model, int _submeshIndex, Vector4 _color)
{
	ModelData* data{ GraphicsResourceManager::Instance().Lookup(_model) };
	if (!data) return;  // 無効ハンドルガード
	if (_submeshIndex < 0 || _submeshIndex >= data->subMeshes.size()) return;  // 範囲チェック
	data->subMeshes[_submeshIndex].material.baseColorFactor = _color;
}
void Gfx::SetTexture(ModelHandle _model, int _submeshIndex, TexHandle _texture)
{
	ModelData* data{ GraphicsResourceManager::Instance().Lookup(_model) };
	if (!data) return;  // 無効ハンドルガード
	if (_submeshIndex < 0 || _submeshIndex >= data->subMeshes.size()) return;  // 範囲チェック
	data->subMeshes[_submeshIndex].material.textures[MaterialTex::BaseColor] = _texture; // 外部テクスチャなのでownerTextureには追加しない
}
void Gfx::SetPostEffect(MaterialHandle _material)
{
	// 空ハンドルは素通しへ
	if (!_material.IsValid())
	{
		currentPostEffectMaterial = {};
		return;
	}
	MaterialData* material{ GraphicsResourceManager::Instance().Lookup(_material) };
	if (!material)
	{
		DEBUG_LOG_ERROR("SetPostEffectに無効なmaterialHandleが渡されました\n");
		currentPostEffectMaterial = {};
		return;
	}
	// 用途があっているかチェック
	if (material->usage != ShaderUsage::PostEffect)
	{
		DEBUG_LOG_ERROR("PostEffect以外のMaterialがSetPostEffectに渡されました\n");
		currentPostEffectMaterial = {};
		return;
	}
	if (!material->pipelineState)
	{
		DEBUG_LOG_ERROR("PostEffect用MaterialにPSOがありません\n");
		currentPostEffectMaterial = {};
		return;
	}
	currentPostEffectMaterial = _material;
}
// 解放
void Gfx::Unload(TexHandle _handle)
{
	GraphicsResourceManager::Instance().Unload(_handle);
}
void Gfx::Unload(ModelHandle _handle)
{
	GraphicsResourceManager::Instance().Unload(_handle);
}
void Gfx::Unload(RTHandle _handle)
{
	GraphicsResourceManager::Instance().Unload(_handle);
}
void Gfx::Unload(ShaderHandle _handle)
{
	GraphicsResourceManager::Instance().Unload(_handle);
}
void Gfx::Unload(MaterialHandle _handle)
{
	if (currentPostEffectMaterial == _handle)
	{
		currentPostEffectMaterial = {};
	}
	GraphicsResourceManager::Instance().Unload(_handle);
}

HWND GfxInternal::GetHWND()
{
	return window.GetHWND();
}

void GfxInternal::SetOnWheel(std::function<void(short)> _func)
{
	window.SetOnWheel(_func);
}