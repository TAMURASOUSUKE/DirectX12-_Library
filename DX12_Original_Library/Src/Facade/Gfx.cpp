#include <cmath>
#include <limits>
#include <utility>
#include "../External/Common/d3dx12.h"
#include "../External/cgltf.h"
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
#include "../Graphics/InternalResource/DefaultFontData.h"
#include "../Animation/AnimationSystem.h"
#include "../Graphics/Primitive3DSystem.h"
#include "GfxInternal.h" // 外部公開しないもの
#include "Gfx.h" // 外部公開するもの

// 無名名前空間で変数を保持する
namespace {
	RTHandle sceneRenderTarget{}; // シーン全体を描画する内部用RenderTarget
	D3D12_CPU_DESCRIPTOR_HANDLE currentRTV{}; // 現在OMSetRenderTargetsで設定しているRTV
	ShaderSystem shaderSystem; // Shader読み込みなどを管理するファイル
	ConstantBufferData orthConstantBufferData; // 正射影行列用定数バッファのデータメンバ
	DynamicBuffer zeroMaterialParameterBuffer{}; // パラメータ未設定スロットへバインドするゼロ埋めCB 全スロットで同じGPUアドレス
	RingConstantBuffer mvpRingCBV; // MVP行列用定数バッファのデータメンバ
	RingConstantBuffer materialRingCBV; // material用定数バッファのデータメンバ
	RingConstantBuffer skinningRingCBV; // スキニング行列定数バッファのデータメンバ
	RingConstantBuffer userMaterialParameterRingCBV{}; // 外部MaterialのユーザーパラメータをGPUへ送るRing, PostEffectとSpriteで将来共有する
	RingConstantBuffer terrainRingCBV{}; // Terrain用RingConstantBuffer
	// 全Terrain描画で共有するグリッド
	VertexBuffer terrainVertexBuffer{};
	IndexBuffer terrainIndexBuffer{};
	Mat4x4 vpMat; // View * Projection
	Mat4x4 mvpMat;
	SpriteBatch fgBatch; // 手前のスプライトバッチ処理
	SpriteBatch bgBatch; // 背景のスプライトバッチ処理
	ShapeBatch shapeBatch; // 基本図形のバッチ処理
	AnimationSystem animSystem; // 3Dモデルのアニメーションシステム
	Primitive3DSystem primitive3DSystem; // 3D基礎図形描画のシステム
	Gfx::BitmapFont defaultFont; // デフォルト用の文字列
	int screenWidth{ 0 }; // 画面の横幅
	int screenHeight{ 0 }; // 画面の縦幅
	int virtualWidth{ 0 };  // ゲーム内で使用する基準幅
	int virtualHeight{ 0 }; // ゲーム内で使用する基準高さ
	bool isSceneRenderTargetActive{ false }; 	// このフレームでシーンRTを描画先として使用できたか
	MaterialHandle currentPostEffectMaterial{}; // 現在画面全体へ適用しているポストエフェクトmaterial(無効ハンドルなら内蔵の素通しPSOを使う)

	constexpr GraphicsPipelineDesc PIPELINE_TABLE[]{
		// 図形塗りつぶし
		{.rootSignatureID = RootSigID::Shape, .pipelineID = PipelineID::ShapeFill,
		  .vs = BuiltinShaderID::ShapeVS, .ps = BuiltinShaderID::ShapePS,
		  .layout = InputLayout::Shape, .blend = BlendMode::Alpha, .depth = DepthParam::None },
		  // 図形ワイヤー
		{.rootSignatureID = RootSigID::Shape, .pipelineID = PipelineID::ShapeWire,
		  .vs = BuiltinShaderID::ShapeVS, .ps = BuiltinShaderID::ShapePS,
		  .layout = InputLayout::Shape, .blend = BlendMode::Alpha, .depth = DepthParam::None,
		  .topology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE },
		  // 3Dモデル
		{.rootSignatureID = RootSigID::Model, .pipelineID = PipelineID::Model,
		 .vs = BuiltinShaderID::ModelVS, .ps = BuiltinShaderID::ModelPS,
		 .layout = InputLayout::Model, .blend = BlendMode::Opaque, .depth = DepthParam::ReadWrite},
		 // テクスチャ 
		 {.rootSignatureID = RootSigID::Texture, .pipelineID = PipelineID::Sprite,
		  .vs = BuiltinShaderID::TextureVS, .ps = BuiltinShaderID::TexturePS,
		  .layout = InputLayout::Sprite, .blend = BlendMode::Alpha, .depth = DepthParam::None},
		  // Terrain
		{.rootSignatureID = RootSigID::Terrain, .pipelineID = PipelineID::TerrainWire,
		.vs = BuiltinShaderID::TerrainVS, .ps = BuiltinShaderID::TerrainPS,
		.hs = BuiltinShaderID::TerrainHS, .ds = BuiltinShaderID::TerrainDS,
		.layout = InputLayout::Texture, .blend = BlendMode::Opaque, .depth = DepthParam::ReadWrite, // textureを流用できる
		.topology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH, .fillMode = D3D12_FILL_MODE_WIREFRAME},// hsとdsを使うのでパッチ系のpipelineという大分類にする , 分割された三角形を確認できるようにワイヤー
		// PostEffect
		{.rootSignatureID = RootSigID::PostEffect, .pipelineID = PipelineID::PostEffect,
		 .vs = BuiltinShaderID::PostEffectVS, .ps = BuiltinShaderID::PostEffectPS,
		 .layout = InputLayout::None, .blend = BlendMode::Opaque, // レイアウトはSV_VertexIDから直接作るので頂点入力はない、Blendも完全に画面を置き換えるのでブレンド無し
		 .depth = DepthParam::None, .topology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE}, // 2D画像を画面へ貼るだけなので深度は使わない
		// 3DPrimitiveFill
		{.rootSignatureID = RootSigID::Primitive3D, .pipelineID = PipelineID::Primitive3DFill,
		 .vs = BuiltinShaderID::Primitive3DVS, .ps = BuiltinShaderID::Primitive3DPS,
		 .layout = InputLayout::Primitive3D, .blend = BlendMode::Opaque,
		 .depth = DepthParam::ReadWrite, .topology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
		 .fillMode = D3D12_FILL_MODE_SOLID},
		 // 3DPrimitiveMeshWire
		{.rootSignatureID = RootSigID::Primitive3D, .pipelineID = PipelineID::Primitive3DMeshWire,
		 .vs = BuiltinShaderID::Primitive3DVS, .ps = BuiltinShaderID::Primitive3DPS,
		 .layout = InputLayout::Primitive3D, .blend = BlendMode::Opaque,
		 .depth = DepthParam::ReadWrite, .topology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
		 .fillMode = D3D12_FILL_MODE_WIREFRAME},
		 // 3DPrimitiveDebugLine
		{.rootSignatureID = RootSigID::Primitive3D, .pipelineID = PipelineID::Primitive3DDebugLine,
		 .vs = BuiltinShaderID::Primitive3DVS, .ps = BuiltinShaderID::Primitive3DPS,
		 .layout = InputLayout::Primitive3D, .blend = BlendMode::Opaque,
		 .depth = DepthParam::ReadOnly, .topology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE,
		 .fillMode = D3D12_FILL_MODE_SOLID}
	};
}

namespace {
	// スキンメッシュ付き
	void DrawSkinnedModel(const AnimInstanceData& _anim, Transform _transform)
	{
		// skinningRingCBVはMAX_BONE_NUM個分しか確保していないため GPUへ送る前に上限を確認する
		if (_anim.skinningMatrices.size() > MAX_BONE_NUM)
		{
			DEBUG_LOG_ERROR("モデルのボーン数が上限を超えています ""boneCount:{} max:{}\n", _anim.skinningMatrices.size(), MAX_BONE_NUM);
			return;
		}

		ModelData* model{ GraphicsResourceManager::Instance().Lookup(_anim.modelHandle) }; // ハンドル分解
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
		terrainRingCBV.Setup(sizeof(TerrainCB));
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

	// 指定したアトラスのセル番号からUV範囲を計算する
	bool TryCalculateAtlasUV(const Gfx::TextureAtlas& _atlas, int _frameIndex, Vector2& _outUVMin, Vector2& _outUVMax)
	{
		// 失敗時に以前の結果が残らないように初期化
		_outUVMax = Vector2::Zero;
		_outUVMin = Vector2::Zero;
		// 無効データなら計算しない
		if (!_atlas.IsValid())
		{
			DEBUG_LOG_ERROR("無効なTextureAtlasが指定されました\n");
			return false;
		}

		// IsValidを通っているのでframeCountは実際に使用できる数
		if (_frameIndex < 0 || _frameIndex >= _atlas.frameCount)
		{
			DEBUG_LOG_ERROR("アトラスのframeIndexは0以上frameCount未満にしてください\n");
			return false;
		}

		// 1次元のセル番号を列と行に変換
		const int column{ _frameIndex % _atlas.columns };
		const int row{ _frameIndex / _atlas.columns };

		// セル一つがテクスチャの何割を占めるか
		const float cellUVWidth{ 1.0f / static_cast<float>(_atlas.columns) };
		const float cellUVHeight{ 1.0f / static_cast<float>(_atlas.rows) };

		// セル左上
		_outUVMin = {column * cellUVWidth, row * cellUVHeight};

		// セル右下
		_outUVMax = {(column + 1) * cellUVWidth, (row + 1) * cellUVHeight};
		return true;
	}

	// UV計算をおこない指定フラグから画像を反転させるなどする
	void ApplySpriteFlip(Gfx::SpriteFlip _flip, Vector2& _uvMin, Vector2& _uvMax)
	{
		switch (_flip)
		{
		case Gfx::SpriteFlip::None:
			// そのまま反転なし
			break;
		case Gfx::SpriteFlip::Horizontal:
			// 水平反転
			std::swap(_uvMin.x, _uvMax.x);
			break;
		case Gfx::SpriteFlip::Vertical:
			// 垂直反転
			std::swap(_uvMin.y, _uvMax.y);
			break;
		case Gfx::SpriteFlip::Both:
			// 両方
			std::swap(_uvMin.x, _uvMax.x);
			std::swap(_uvMin.y, _uvMax.y);
			break;
		default:
			break;
		}
	}

	// 3D基礎図形の描画方法を内部の型に変換するヘルパー
	Primitive3DDrawMode ConvertPrimitive3DStyle(Gfx::Primitive3DStyle _style)
	{
		switch (_style)
		{
		case Gfx::Primitive3DStyle::Fill:
			return Primitive3DDrawMode::Fill;
		case Gfx::Primitive3DStyle::MeshWireframe:
			return Primitive3DDrawMode::MeshWireframe;
		case Gfx::Primitive3DStyle::DebugLine:
			return Primitive3DDrawMode::DebugLine;
		default:
			return Primitive3DDrawMode::Fill;
		}
	}

	// Gfx内のメンバの掃除
	void ShutdownGfxOwnedResources()
	{
		// 仮で作っているTerrainのVB.IBを解放する(これは一時的な物なので3Dの基本図形描画時になくなる予定)
		terrainIndexBuffer = IndexBuffer{};
		terrainVertexBuffer = VertexBuffer{};
		animSystem.Shutdown();
		// RingConstantBufferの解放
		mvpRingCBV.Shutdown();
		materialRingCBV.Shutdown();
		skinningRingCBV.Shutdown();
		terrainRingCBV.Shutdown();
		userMaterialParameterRingCBV.Shutdown();
		zeroMaterialParameterBuffer = DynamicBuffer{}; // 解放
		// Batchが所有するVB,IBを解放
		fgBatch.Shutdown();
		bgBatch.Shutdown();
		shapeBatch.Shutdown();
		primitive3DSystem.Shutdown();

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
bool GfxInternal::Initialize(HWND _hwnd, int _clientWidth, int _clientHeight, int _virtualWidth, int _virtualHeight)
{
	if (_clientWidth <= 0 || _clientHeight <= 0 || _virtualWidth <= 0 || _virtualHeight <= 0)
	{
		DEBUG_LOG_ERROR("ウィンドウサイズと仮想解像度には0より大きい値を指定してください\n");
		return false;
	}

	screenWidth = _clientWidth;
	screenHeight = _clientHeight;
	virtualWidth = _virtualWidth;
	virtualHeight = _virtualHeight;
	// 前回の初期化状態を引き継がない
	currentPostEffectMaterial = {};

	DEBUG_LOG("ClientSize = {} x {}\n", _clientWidth, _clientHeight);


	GraphicsDevice::Instance().Setup(_hwnd, _clientWidth, _clientHeight); // デバイスの初期化
	if (!GraphicsDevice::Instance().GetDevice())
	{
		DEBUG_LOG_ERROR("デバイスの読み込みに失敗しました\n");
		return false; // デバイス読み込み失敗したらfalse
	}

	DescriptorManager::Instance().Setup(GraphicsDevice::Instance().GetDevice()); // ディスクリプタマネージャーをデバイスを使って初期化

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

	GraphicsResourceManager::Instance().Setup(GraphicsDevice::Instance().GetDevice()); // リソース管理ファイルの初期化
	
	// 画面と同じサイズの内部描画先を作成
	sceneRenderTarget = GraphicsResourceManager::Instance().CreateRenderTarget(static_cast<UINT>(_clientWidth), static_cast<UINT>(_clientHeight));
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
	const Mat4x4 orthMat{ Mat4x4::MakeOrthGraphic(static_cast<float>(_virtualWidth), static_cast<float>(_virtualHeight)) }; // 変換行列の作成
	orthConstantBufferData = GraphicsResourceManager::Instance().CreateConstantBuffer(&orthMat, sizeof(Mat4x4));

	// 透視投影行列の作成(一旦ハードコーディング)
	const float virtualAspect{ static_cast<float>(_virtualWidth) / static_cast<float>(_virtualHeight) };
	vpMat = Mat4x4::MakeLookAt({ 0.0f, 1.0f, -3.0f }, { 0.0f, 1.0f, 1.0f }, Vector3::Up) * Mat4x4::MakePerspective(60.0f * Math::DEG_TO_RAD, virtualAspect, 0.1f, 100.0f);
	mvpRingCBV.Setup(sizeof(Mat4x4)); // リングバッファ初期化
	materialRingCBV.Setup(sizeof(MaterialCB));  // materialのリング定数バッファを初期化
	skinningRingCBV.Setup(sizeof(Mat4x4) * MAX_BONE_NUM); // ボーン用の定数バッファを更新
	userMaterialParameterRingCBV.Setup(static_cast<UINT>(MAX_MATERIAL_PARAMETER_SIZE), static_cast<UINT>(MAX_MATERIAL_PARAMETER_UPDATE_PER_FRAME));

	// ゼロダミーCBの作成(未設定のMaterialパラメータを安全に0として読ませる)
	zeroMaterialParameterBuffer = GraphicsResourceManager::Instance().CreateDynamicBuffer(static_cast<UINT>(MAX_MATERIAL_PARAMETER_SIZE));
	if (!zeroMaterialParameterBuffer.resource || !zeroMaterialParameterBuffer.mappedPtr)
	{
		DEBUG_LOG_ERROR("MaterialParamter用ゼロダミーCBの作成に失敗しました\n");
		return false;
	}
	//CreateDynamicBufferした後の未定義の中身に対して明示的に0クリアを入れる
	std::memset(zeroMaterialParameterBuffer.mappedPtr, 0, MAX_MATERIAL_PARAMETER_SIZE);
	animSystem.Setup(); // アニメーションシステムのセットアップ
	// スプライトバッチ処理初期化
	fgBatch.Setup(shaderSystem.GetRootSignature(RootSigID::Texture), shaderSystem.GetPipeline(PipelineID::Sprite), orthConstantBufferData.resource.Get());
	bgBatch.Setup(shaderSystem.GetRootSignature(RootSigID::Texture), shaderSystem.GetPipeline(PipelineID::Sprite), orthConstantBufferData.resource.Get());
	shapeBatch.Setup(shaderSystem.GetRootSignature(RootSigID::Shape),shaderSystem.GetPipeline(PipelineID::ShapeFill), shaderSystem.GetPipeline(PipelineID::ShapeWire), orthConstantBufferData.resource.Get());
	if (!primitive3DSystem.Setup(shaderSystem.GetRootSignature(RootSigID::Primitive3D), shaderSystem.GetPipeline(PipelineID::Primitive3DFill), shaderSystem.GetPipeline(PipelineID::Primitive3DMeshWire), shaderSystem.GetPipeline(PipelineID::Primitive3DDebugLine)))
	{
		DEBUG_LOG_ERROR("Primitive3DSystemの初期化に失敗しました\n");
		return false;
	}

	// 文字列構造体初期化
	defaultFont.texture = GraphicsResourceManager::Instance().LoadTextureFromMemory(InternalResource::defaultFontPng, InternalResource::defaultFontPngSize, false); // デフォルトフォント
	if (!defaultFont.texture.IsValid())
	{
		DEBUG_LOG_ERROR("内蔵デフォルトフォントの読み込みに失敗しました\n");
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
	primitive3DSystem.Reset();
	// 定数バッファのカウンターリセット
	mvpRingCBV.Reset();
	materialRingCBV.Reset();
	skinningRingCBV.Reset();
	terrainRingCBV.Reset();
	userMaterialParameterRingCBV.Reset();

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
		bgBatch.Flush(userMaterialParameterRingCBV, zeroMaterialParameterBuffer.resource.Get());
	}
	// 3D基礎図形
	{
		GPU_MARKER("Primitive3D");
		if (!primitive3DSystem.Flush(vpMat)) DEBUG_LOG_ERROR("Primitive3Dの更新に失敗しました\n");
	}
	{
		GPU_MARKER("foreGround");
		fgBatch.Flush(userMaterialParameterRingCBV, zeroMaterialParameterBuffer.resource.Get());
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
			// パラメータを取り出すために使われているmaterialDataも保持する
			MaterialData* activePostEffectMaterial{ nullptr }; // このendframeないだけで利用
			// 外部materialが設定されている場合は台帳から取得
			if (currentPostEffectMaterial.IsValid())
			{
				MaterialData* material{ GraphicsResourceManager::Instance().Lookup(currentPostEffectMaterial) };
				// 内部要素をチェックして全て通っていたらPSO差し替え
				if (material && material->usage == ShaderUsage::PostEffect && material->pipelineState)
				{
					postEffectPipeline = material->pipelineState.Get();
					activePostEffectMaterial = material;
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
			constexpr UINT POSTEFFECT_MATERIAL_ROOT_PARAM_BASE{ 1 }; // RootParamの0はシーンRTのため1から始まるようにする
			D3D12_GPU_VIRTUAL_ADDRESS zeroParameterAddress{ zeroMaterialParameterBuffer.resource->GetGPUVirtualAddress() }; // 全ての未設定スロットで共有するGPUアドレス
			
			// GPUへの送信
			for (size_t i = 0; i < MATERIAL_PARAMETER_SLOT_COUNT; i++)
			{
				// 未設定時は必ずゼロダミーCBにする
				D3D12_GPU_VIRTUAL_ADDRESS parameterAddress{ zeroParameterAddress };
				if (activePostEffectMaterial)
				{
					const MaterialParameterBlock& parameter{ activePostEffectMaterial->parameters[i] };
					if (parameter.hasParameter)
					{
						// materialが設定されていた場合
						// setup時に0クリアされているので256byte全体を送る
						const D3D12_GPU_VIRTUAL_ADDRESS updateAddress{ userMaterialParameterRingCBV.Update(parameter.parameterData.data(), static_cast<UINT>(parameter.parameterData.size())) };
						
						if (updateAddress != 0)
						{
							parameterAddress = updateAddress;
						}
						else
						{
							// Ringの上限超過が起きた場合は不正なGPUアドレスを設定せずにゼロダミーへ戻す
							DEBUG_LOG_ERROR("MaterialParameterのGPU転送に失敗しました Slot = {}\n", i);
						}
					}
				}

				// Slot0ならRootParam[1]へ設定してHLSL側でb4
				// Slot1ならRootParam[2]へ設定してHLSL側でb5
				cmd->SetGraphicsRootConstantBufferView(POSTEFFECT_MATERIAL_ROOT_PARAM_BASE + static_cast<UINT>(i), parameterAddress);
			}
			
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
	GraphicsDevice& graphicsDevice{ GraphicsDevice::Instance() };
	const bool endFrameSucceeded{ graphicsDevice.EndFrame() };
	// Signalが成功したフレームだけpendingReleaseへ確定済みのFence値を割り当てる
	if (endFrameSucceeded) GraphicsResourceManager::Instance().CommitPendingRelease(GraphicsDevice::Instance().GetLastSubmittedFenceValue()); 	// このフレーム中にUnloadされたリソースに対して今回のSignalしたFence値を割り当てる
	else DEBUG_LOG_ERROR("GraphicsDevice::EndFrameに失敗したため遅延解放のFence確定を見送ります\n"); // fenceが成立していないので次の正常フレーム若しくは終了処理まで保持する
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

bool Gfx::Detail::SetMaterialParameterRaw(MaterialHandle _handle, std::size_t _slot, const void* _data, size_t _dataSize)
{
	GraphicsResourceManager& resourceManager{ GraphicsResourceManager::Instance() };
	MaterialData* material { resourceManager.Lookup(_handle) };
	if (!material)
	{
		DEBUG_LOG_ERROR("SetMaterialParameterに無効なMaterialHandleが渡されました\n");
		return false;
	}

	switch (material->usage)
	{
	case ShaderUsage::PostEffect:
	case ShaderUsage::Sprite:
		break; // GPUへの配達に対応している
	case ShaderUsage::Model:
		DEBUG_LOG_ERROR("Model用MaterialParameterはまだ対応していません\n");
		return false;
	default:
		DEBUG_LOG_ERROR("不明なShaderUsageです\n");
		return false;
	}

	return resourceManager.SetMaterialParameter(_handle, _slot, _data, _dataSize);
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

Gfx::TextureAtlas Gfx::LoadTextureAtlas(const char* _filePath, int _columns, int _rows, int _frameCount)
{
	if (!_filePath || _filePath[0] == '\0')
	{
		DEBUG_LOG_ERROR("アトラス画像のファイルパスが空です\n");
		return {};
	}

	// 0除算を防ぐため、分割数を先に検査する
	if (_columns <= 0 || _rows <= 0)
	{
		DEBUG_LOG_ERROR("アトラス画像のcolumnsとrowsには1以上を指定してください\n");
		return {};
	}

	// オーバーフロー対策のlonglong
	const long long capacity{ static_cast<long long>(_columns) * _rows };

	// intで表現できないセル数はここで拒否する
	if (capacity > (std::numeric_limits<int>::max)())
	{
		DEBUG_LOG_ERROR("アトラス画像のセル総数がintの上限を超えています\n");
		return {};
	}

	// 0なら全セルを利用する
	const long long useFrameCount{ _frameCount == 0 ? capacity : static_cast<long long>(_frameCount) };

	if (useFrameCount <= 0 || useFrameCount > capacity)
	{
		DEBUG_LOG_ERROR("frameCountには1以上かつセル総数以下を指定してください\n");
		return {};
	}

	// 実際のGPUリソース作成は既存関数へ任せる
	const TexHandle texture{ LoadTexture(_filePath, TextureUsage::Color) };
	if (!texture.IsValid()) return {};
	return TextureAtlas{ texture, _columns, _rows, static_cast<int>(useFrameCount) };
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
			DEBUG_LOG_ERROR("VertexShaderHandleが無効です\n");			return MaterialHandle{};
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

AnimInstanceHandle Gfx::CreateAnimInstance(ModelHandle _handle)
{
	return animSystem.Create(_handle);
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
void Gfx::DrawString(const char* _string, Vector2 _position, float _scale, Vector4 _color, RenderLayer _layer)
{
	DrawString(defaultFont, _string, _position, _scale, _color, _layer);
}

// 文字列描画(フォント設定用)
void Gfx::DrawString(const BitmapFont& _font, const char* _string, Vector2 _position, float _scale, Vector4 _color, RenderLayer _layer)
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


		DrawSpriteSized(_font.texture, cursor, glyphSize, 0.0f, SpriteFlip::None, _color, uvMin, uvMax, _layer);

		cursor.x += glyphSize.x; // 書いた分右へ
	}
}

void Gfx::DrawSprite(TexHandle _texture, Vector2 _position, Vector2 _scale, float _radRotation, SpriteFlip _flip, Vector4 _color, Vector2 _uvMin, Vector2 _uvMax, RenderLayer _layer)
{
	// materialなしは空のマテリアルを渡して共通処理へ
	DrawSprite(_texture, _position, MaterialHandle{}, _scale, _radRotation, _flip, _color, _uvMin, _uvMax, _layer);
}

void Gfx::DrawSprite(const TextureAtlas& _atlas, int _frameIndex, Vector2 _position, Vector2 _scale, float _radRotation, SpriteFlip _flip, Vector4 _color, RenderLayer _layer)
{
	Vector2 uvMin{};
	Vector2 uvMax{};
	// UVが作れない場合はこのスプライトの描画をあきらめる
	if (!TryCalculateAtlasUV(_atlas, _frameIndex, uvMin, uvMax)) return;
	// 既存のDrawに任せる
	DrawSprite(_atlas.texture, _position, _scale, _radRotation, _flip, _color, uvMin, uvMax, _layer);
}

void Gfx::DrawSpriteSized(TexHandle _texture, Vector2 _position, Vector2 _pixelSize, float _radRotation, SpriteFlip _flip, Vector4 _color, Vector2 _uvMin, Vector2 _uvMax, RenderLayer _layer)
{
	// 通常は空のMaterialHandleを渡す
	DrawSpriteSized(_texture, _position, _pixelSize, MaterialHandle{}, _radRotation, _flip, _color, _uvMin, _uvMax, _layer);
}

void Gfx::DrawSpriteSized(const TextureAtlas& _atlas, int _frameIndex, Vector2 _position, Vector2 _pixelSize, float _radRotation, SpriteFlip _flip, Vector4 _color, RenderLayer _layer)
{
	Vector2 uvMin{};
	Vector2 uvMax{};
	// 指定されたセル番号からUV範囲を求める
	if (!TryCalculateAtlasUV(_atlas, _frameIndex, uvMin, uvMax)) return;
	// TexHandle版へ
	DrawSpriteSized(_atlas.texture, _position, _pixelSize, _radRotation, _flip, _color, uvMin, uvMax, _layer);
}

void Gfx::DrawSprite(TexHandle _texture, Vector2 _position, MaterialHandle _material, Vector2 _scale, float _radRotation, SpriteFlip _flip, Vector4 _color, Vector2 _uvMin, Vector2 _uvMax, RenderLayer _layer)
{
	// 反転はUVの入れ替えで行えるので倍数には負数を許可しない
	if (_scale.x < 0.0f || _scale.y < 0.0f)
	{
		DEBUG_LOG_ERROR("Spriteの倍率には0以上の値を指定してください\n");
		return;
	}

	TextureData* data{ GraphicsResourceManager::Instance().Lookup(_texture) };
	if (!data) return;

	// UVが画像全体の何割を使用しているか求める
	const float uvWidth{ std::abs(_uvMax.x - _uvMin.x) };
	const float uvHeight{ std::abs(_uvMax.y - _uvMin.y) };

	// 画像原寸 * UV使用範囲 * XY倍率
	const Vector2 pixelSize{ static_cast<float>(data->width) * uvWidth * _scale.x, static_cast<float>(data->height) * uvHeight * _scale.y};
	// 計算後は渡す
	DrawSpriteSized(_texture, _position, pixelSize, _material, _radRotation,  _flip, _color, _uvMin, _uvMax, _layer);
}

void Gfx::DrawSprite(const TextureAtlas& _atlas, int _frameIndex, Vector2 _position, MaterialHandle _material, Vector2 _scale, float _radRotation, SpriteFlip _flip, Vector4 _color, RenderLayer _layer)
{
	Vector2 uvMin{};
	Vector2 uvMax{};

	// 指定されたセル番号からUV範囲を求める
	if (!TryCalculateAtlasUV(_atlas, _frameIndex, uvMin, uvMax)) return;
	// 既存に任せる
	DrawSprite(_atlas.texture, _position, _material, _scale, _radRotation, _flip, _color, uvMin, uvMax, _layer);
}

void Gfx::DrawSpriteSized(TexHandle _texture, Vector2 _position, Vector2 _pixelSize, MaterialHandle _material, float _radRotation, SpriteFlip _flip, Vector4 _color, Vector2 _uvMin, Vector2 _uvMax, RenderLayer _layer)
{
	ID3D12PipelineState* usePipeline{ shaderSystem.GetPipeline(PipelineID::Sprite) }; // 最初は内蔵SpritePSO
	const MaterialParameterSet* useParameters{ nullptr }; // 内蔵Spriteならnull
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
			// RegisterSprite内で値をコピーするためフレーム中は保持しない
			useParameters = &material->parameters;
		}
	}

	// この関数の引数が値渡しなのでここでFlip関数内で入れ替えても呼び出し元の値には影響がない
	ApplySpriteFlip(_flip, _uvMin, _uvMax);

	switch (_layer)
	{
	case RenderLayer::BackGround:
		bgBatch.RegisterSprite(_texture, usePipeline, useParameters, _position, _pixelSize, _radRotation, _color, _uvMin, _uvMax);
		break;
	case RenderLayer::ForeGround:
		fgBatch.RegisterSprite(_texture, usePipeline, useParameters, _position, _pixelSize, _radRotation, _color, _uvMin, _uvMax);
		break;
	default:
		break;
	}
}

void Gfx::DrawSpriteSized(const TextureAtlas& _atlas, int _frameIndex, Vector2 _position, Vector2 _pixelSize, MaterialHandle _material, float _radRotation, SpriteFlip _flip, Vector4 _color, RenderLayer _layer)
{
	Vector2 uvMin{};
	Vector2 uvMax{};

	// 指定されたセル番号からUV範囲を求める
	if (!TryCalculateAtlasUV(_atlas, _frameIndex, uvMin, uvMax)) return;
	// 既存に任せる
	DrawSpriteSized(_atlas.texture, _position, _pixelSize, _material, _radRotation, _flip, _color, uvMin, uvMax, _layer);
}

bool Gfx::UpdateSpriteAnim(const TextureAtlas& _atlas, SpriteAnimationState& _state, float _deltaTime)
{
	if (!_state.IsValid(_atlas))
	{
		DEBUG_LOG_ERROR("SpriteAnimationStateの設定が不正です\n");
		return false;
	}

	if (_deltaTime <= 0.0f || _state.isFinished)
	{
		return true;
	}

	_state.elapsedTime += _deltaTime;

	// 処理落ちで複数コマ分の時間が経過しても追いつける
	while (_state.elapsedTime >= _state.secondsPerFrame)
	{
		_state.elapsedTime -= _state.secondsPerFrame;
		_state.currentFrame++;

		// まだ範囲内なら次のコマへ
		if (_state.currentFrame <= _state.lastFrame)
		{
			continue; // コマを切り替えたら飛ばす
		}

		if (_state.isLoop)
		{
			// ループ指定なら最初に戻る
			_state.currentFrame = _state.firstFrame;
		}
		else
		{
			// 非ループなら指定範囲の最後で停止する
			_state.currentFrame = _state.lastFrame;
			_state.elapsedTime = 0.0f; // 経過時間リセット
			_state.isFinished = true; // 終了したフラグを立てる
			break; // 終了したので抜ける
		}
	}
	return true;
}

void Gfx::DrawCube3D(const Transform& _transform, Vector4 _color, Primitive3DStyle _style)
{
	if (!primitive3DSystem.RegisterCube(_transform, _color, ConvertPrimitive3DStyle(_style)))
	{
		DEBUG_LOG_ERROR("Cubeの描画に失敗しました\n");
		return;
	}
}

void Gfx::DrawSphere3D(const Transform& _transform, Vector4 _color, Primitive3DStyle _style)
{
	if (!primitive3DSystem.RegisterSphere(_transform, _color, ConvertPrimitive3DStyle(_style)))
	{
		DEBUG_LOG_ERROR("Sphereの描画に失敗しました\n");
		return;
	}
}

void Gfx::DrawCylinder3D(const Transform& _transform, Vector4 _color, Primitive3DStyle _style)
{
	if (!primitive3DSystem.RegisterCylinder(_transform, _color, ConvertPrimitive3DStyle(_style)))
	{
		DEBUG_LOG_ERROR("Cylinderの描画に失敗しました\n");
		return;
	}
}

void Gfx::DrawCapsule3D(Vector3 _start, Vector3 _end, float _radius, Vector4 _color, Primitive3DStyle _style)
{
	if (!primitive3DSystem.RegisterCapsule(_start, _end, _radius, _color, ConvertPrimitive3DStyle(_style)))
	{
		DEBUG_LOG_ERROR("Capsuleの描画に失敗しました\n");
		return;
	}
}

void Gfx::DrawPlane3D(const Transform& _transform, Vector4 _color, Primitive3DStyle _style)
{
	if (!primitive3DSystem.RegisterPlane(_transform, _color, ConvertPrimitive3DStyle(_style)))
	{
		DEBUG_LOG_ERROR("Planeの描画に失敗しました\n");
		return;
	}
}

void Gfx::DrawLine3D(Vector3 _start, Vector3 _end, Vector4 _color)
{
	if (primitive3DSystem.RegisterLine(_start, _end, _color))
	{
		DEBUG_LOG_ERROR("Lineの描画に失敗しました\n");
		return;
	}
}

void Gfx::DrawGrid3D(Vector3 _center, Quaternion _rotation,unsigned int _halfCellCount, float _cellSize, Vector4 _color)
{
	if (!primitive3DSystem.RegisterGrid(_center, _rotation, _halfCellCount, _cellSize, _color))
	{
		DEBUG_LOG_ERROR("Gridの描画に失敗しました\n");
		return;
	}
}

void Gfx::DrawWorldAxisGrid(Vector3 _center, unsigned int _halfCellCount, float _cellSize, Vector4 _xAxisColor, Vector4 _yAxisColor, Vector4 _zAxisColor)
{
	if (!primitive3DSystem.RegisterWorldAxisGrid(_center, _halfCellCount, _cellSize, _xAxisColor, _yAxisColor, _zAxisColor))
	{
		DEBUG_LOG_ERROR("WorldAxisGridの描画に失敗しました\n");
		return;
	}
}

void Gfx::DrawModel(ModelHandle _model, Transform _transform)
{
	{
		// マクロがスコープを抜けるとEndEventするので囲う
		GPU_MARKER("backGround");
		bgBatch.Flush(userMaterialParameterRingCBV, zeroMaterialParameterBuffer.resource.Get()); // 背景の上に来るように3D描画前には背景batchをFlushする
	}

	// 今の状態では静的モデルだけ
	DrawStaticModel(_model, _transform);
}

void Gfx::DrawAnimatedModel(AnimInstanceHandle _handle, Transform _transform)
{
	AnimInstanceData* instance{ animSystem.Lookup(_handle) };
	if (!instance) return;
	{
		// マクロがスコープを抜けるとEndEventするので囲う
		GPU_MARKER("backGround");
		bgBatch.Flush(userMaterialParameterRingCBV, zeroMaterialParameterBuffer.resource.Get()); // 背景の上に来るように3D描画前には背景batchをFlushする
	}
	DrawSkinnedModel(*instance, _transform);
}

bool Gfx::UpdateAnim(AnimInstanceHandle _handle, float _deltaTime)
{
	return animSystem.Update(_handle, _deltaTime);
}

bool Gfx::PlayAnim(AnimInstanceHandle _handle, int _clipIndex, bool _isLoop, float _playbackSpeed)
{
	return animSystem.Play(_handle, _clipIndex, _isLoop, _playbackSpeed);
}

bool Gfx::PlayRangeAnim(AnimInstanceHandle _handle, int _clipIndex, float _startTime, float _endTime, bool _isLoop, float _playbackSpeed)
{
	return animSystem.PlayRange(_handle, _clipIndex, _startTime, _endTime, _isLoop, _playbackSpeed);
}

bool Gfx::ResumeAnim(AnimInstanceHandle _handle)
{
	return animSystem.ResumePlay(_handle);
}

bool Gfx::StopAnim(AnimInstanceHandle _handle)
{
	return animSystem.Stop(_handle);
}

bool Gfx::PauseAnim(AnimInstanceHandle _handle)
{
	return animSystem.Pause(_handle);
}

bool Gfx::IsFinishedAnim(AnimInstanceHandle _handle)
{
	return animSystem.IsFinished(_handle);
}

bool Gfx::SetAnimPlaybackSpeed(AnimInstanceHandle _handle, float _playbackSpeed)
{
	return animSystem.SetAnimPlaybackSpeed(_handle, _playbackSpeed);
}

bool Gfx::DestroyAnim(AnimInstanceHandle _handle)
{
	return animSystem.Destroy(_handle);
}

void Gfx::DrawTerrain(Vector3 _position, float _scale, float _tessFactor, float _heightScale, Vector4 _color, TexHandle _heightMap)
{
	{
		// マクロがスコープを抜けるとEndEventするので囲う
		GPU_MARKER("backGround");
		bgBatch.Flush(userMaterialParameterRingCBV, zeroMaterialParameterBuffer.resource.Get()); // 背景の上に来るように3D描画前には背景batchをFlushする
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
