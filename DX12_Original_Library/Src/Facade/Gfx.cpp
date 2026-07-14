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
	ShaderSystem shaderSystem; // Shader読み込みなどを管理するファイル
	ConstantBufferData orthConstantBufferData; // 正射影行列用定数バッファのデータメンバ
	RingConstantBuffer mvpRingCBV; // MVP行列用定数バッファのデータメンバ
	RingConstantBuffer materialRingCBV; // material用定数バッファのデータメンバ
	RingConstantBuffer skinningRingCBV; // スキニング行列定数バッファのデータメンバ
	Mat4x4 vpMat; // View * Projection
	Mat4x4 mvpMat;
	SpriteBatch fgBatch; // 手前のスプライトバッチ処理
	SpriteBatch bgBatch; // 背景のスプライトバッチ処理
	ShapeBatch shapeBatch; // 基本図形のバッチ処理
	Gfx::BitmapFont defaultFont; // デフォルト用の文字列
	int screenWidth = 0; // 画面の横幅
	int screenHeight = 0; // 画面の縦幅

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
		  .layout = InputLayout::Texture, .blend = BlendMode::Alpha, .depth = DepthParam::None},
	};
}

namespace {
	// スキンメッシュ付き
	void DrawSkinnedModel(AnimInstanceData& _anim, Transform _transform)
	{
		GraphicsResourceManager::Instance().UpdateGlobalPose(_anim);

		ModelData* model{ GraphicsResourceManager::Instance().Lookup(_anim.handle) }; // ハンドル分解
		if (!model) return;

		auto cmd{ GraphicsDevice::Instance().GetCommandList() };
		Mat4x4 worldMat{ _transform.GetWorldMatrix() }; // ワールド行列の取得
		mvpMat = worldMat * vpMat;

		cmd->SetGraphicsRootSignature(shaderSystem.GetRootSignature(RootSigID::Model));
		cmd->SetPipelineState(shaderSystem.GetPipeline(PipelineID::Model));

		DescriptorManager::Instance().SetDiscriptor(cmd);
		cmd->SetGraphicsRootConstantBufferView(0, mvpRingCBV.Update(&mvpMat, sizeof(Mat4x4))); // MVP更新
		cmd->SetGraphicsRootConstantBufferView(2, skinningRingCBV.Update(_anim.skinningMatrices.data(), sizeof(Mat4x4) * static_cast<UINT>(_anim.skinningMatrices.size()))); // ボーンを更新
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
			cmd->SetGraphicsRootConstantBufferView(1, materialRingCBV.Update(&matCB, sizeof(MaterialCB)));

			TextureData* tex{ GraphicsResourceManager::Instance().Lookup(sub.material.textures[MaterialTex::BaseColor]) };
			if (tex) cmd->SetGraphicsRootDescriptorTable(3, tex->srvHandle.gpu);

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
		cmd->SetGraphicsRootConstantBufferView(0, mvpRingCBV.Update(&mvpMat, sizeof(Mat4x4)));
		// 静的描画の場合は単位行列を送る
		cmd->SetGraphicsRootConstantBufferView(2, skinningRingCBV.Update(&Mat4x4::Identity, sizeof(Mat4x4)));
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
			// Ringで送ってb1にバインドする
			cmd->SetGraphicsRootConstantBufferView(1, materialRingCBV.Update(&matCB, sizeof(MaterialCB)));

			// テクスチャをバインド
			TextureData* tex{ GraphicsResourceManager::Instance().Lookup(sub.material.textures[MaterialTex::BaseColor]) };
			if (tex) cmd->SetGraphicsRootDescriptorTable(3, tex->srvHandle.gpu);

			// 頂点インデックスをバインド
			cmd->IASetVertexBuffers(0, 1, &sub.vertexBuffer.vertexView);
			cmd->IASetIndexBuffer(&sub.indexBuffer.indexView);

			cmd->DrawIndexedInstanced(sub.indexBuffer.indexCount, 1, 0, 0, 0);
		}
	}
}

// 初期化処理(これを呼ぶだけで初期化処理が済むようにする)
bool GfxInternal::Initialize(const wchar_t* _title, int _width, int _height)
{
	HRESULT result{};

	screenWidth = _width;
	screenHeight = _height;

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

	shaderSystem.Initialize(GraphicsDevice::Instance().GetDevice()); // ShaderSystemの初期化

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

	// ピクセル座標からNDC座標へ変換
	Mat4x4 orthMat{ Mat4x4::MakeOrthGraphic(static_cast<float>(_width), static_cast<float>(_height)) }; // 変換行列の作成
	orthConstantBufferData = GraphicsResourceManager::Instance().CreateConstantBuffer(&orthMat, sizeof(Mat4x4));

	// 透視投影行列の作成(一旦キューブが描画できるのを確認するためにハードコーディング)
	vpMat = Mat4x4::MakeLookAt({ 0.0f, 1.0f, -3.0f }, { 0.0f, 1.0f, 0.0f }, Vector3::Up) * Mat4x4::MakePerspective(60.0f * Math::DEG_TO_RAD, static_cast<float>(screenWidth) / static_cast<float>(screenHeight), 0.1f, 100.0f);
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

// メッセージループ
bool Gfx::ProcessMessage()
{
	MSG msg{}; // イベント情報を格納する型
	while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
	{
		if (msg.message == WM_QUIT) return false;
		DispatchMessage(&msg);
	}
	return true;
}

// フレーム開始処理
void GfxInternal::BeginFrame()
{
	GraphicsDevice::Instance().BeginFrame(); // フレームの最初の処理

	// batch処理のカウンターリセット
	bgBatch.Reset();
	fgBatch.Reset();
	shapeBatch.Reset();
	// 定数バッファのカウンターリセット
	mvpRingCBV.Reset();
	materialRingCBV.Reset();
	skinningRingCBV.Reset();

	auto cmdList{ GraphicsDevice::Instance().GetCommandList() }; // コマンドリスト
	auto rtv{ GraphicsDevice::Instance().GetCurrentRTV() }; // 現在のRTV
	auto dsv{ GraphicsDevice::Instance().GetDSV() };

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
	cmdList->OMSetRenderTargets(1, &rtv, false, &dsv);

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
	GraphicsDevice::Instance().EndFrame(); // フレームの最後の処理
}

// 終了処理
void GfxInternal::Finish()
{
	shaderSystem.Shutdown();
	DescriptorManager::Instance().Shutdown();
	GraphicsDevice::Instance().Shutdown();
}

// 描画先をクリアする(色指定可能)
void Gfx::ClearScreen(float _r, float _g, float _b, float _a)
{
	float windowColor[]{ _r, _g, _b, _a };
	GraphicsDevice::Instance().GetCommandList()->ClearRenderTargetView(GraphicsDevice::Instance().GetCurrentRTV(), windowColor, 0, nullptr); // コマンドリストを取得しそこから現在書き込んでいるRTVにの色を任意色でクリアする
}

// 画像読み込み
TexHandle Gfx::LoadTexture(const char* _filePath)
{
	return GraphicsResourceManager::Instance().LoadTexture(_filePath);
}

// モデル読み込み
ModelHandle Gfx::LoadModel(const char* _filePath)
{
	return GraphicsResourceManager::Instance().LoadModel(_filePath);
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
void Gfx::DrawString(const char* _string, Vector2 _position, float _scale, LenderLayer _layer)
{
	DrawString(defaultFont, _string, _position, _scale, _layer);
}

// 文字列描画(フォント設定用)
void Gfx::DrawString(const BitmapFont& _font, const char* _string, Vector2 _position, float _scale, LenderLayer _layer)
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


		DrawSprite(_font.texture, cursor, glyphSize, 0.0f, uvMin, uvMax, _layer);

		cursor.x += glyphSize.x; // 書いた分右へ
	}
}

// 画像登録
void Gfx::DrawSprite(TexHandle _texture, Vector2 _position, Vector2 _size, float _radRotation, Vector2 _uvMin, Vector2 _uvMax, LenderLayer _layer)
{
	switch (_layer)
	{
	case LenderLayer::BackGround:
		bgBatch.RegisterSprite(_texture, _position, _size, _radRotation, _uvMin, _uvMax);
		break;
	case LenderLayer::ForeGround:
		fgBatch.RegisterSprite(_texture, _position, _size, _radRotation, _uvMin, _uvMax);
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

void Gfx::SetBaseColor(ModelHandle model, int submeshIndex, Vector4 color)
{
	ModelData* data{ GraphicsResourceManager::Instance().Lookup(model) };
	if (!data) return;  // 無効ハンドルガード
	if (submeshIndex < 0 || submeshIndex >= data->subMeshes.size()) return;  // 範囲チェック
	data->subMeshes[submeshIndex].material.baseColorFactor = color;
}
void Gfx::SetTexture(ModelHandle model, int submeshIndex, TexHandle texture)
{
	ModelData* data{ GraphicsResourceManager::Instance().Lookup(model) };
	if (!data) return;  // 無効ハンドルガード
	if (submeshIndex < 0 || submeshIndex >= data->subMeshes.size()) return;  // 範囲チェック
	data->subMeshes[submeshIndex].material.textures[MaterialTex::BaseColor] = texture;
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

HWND GfxInternal::GetHWND()
{
	return window.GetHWND();
}

void GfxInternal::SetOnWheel(std::function<void(short)> _func)
{
	window.SetOnWheel(_func);
}