#include "../External/Common/d3dx12.h"
#include "../External/cgltf.h"
#include "../Window/Window.h"
#include "../Debug/DebugLogs.h"
#include "../Graphics/GraphicsDevice.h"
#include "../Graphics/DescriptorManager.h"
#include "../Graphics/ShaderSystem.h"
#include "../Graphics/ResourceManager.h"
#include "../Graphics/SpriteBatch.h"
#include "../Graphics/RingConstantBuffer.h"
#include "../Graphics/DrawDebug/DebugTriangle.h"
#include "../Graphics/DrawDebug/DebugQuad.h"
#include "../Graphics/DrawDebug/DebugCube.h"
#include "../Graphics/GPUMarker.h"
#include "../Math/TSMath.h"
#include "../Graphics/GraphicsType.h"
#include "GfxInternal.h" // 外部公開しないもの
#include "Gfx.h" // 外部公開するもの

// 無名名前空間で変数を保持する
namespace {
	Window window; // window作成クラス
	ShaderSystem shaderSystem; // Shader読み込みなどを管理するファイル
	ConstantBufferData orthConstantBufferData; // 正射影行列用定数バッファのデータメンバ
	RingConstantBuffer mvpRingCBV; // MVP行列用定数バッファのデータメンバ
	Mat4x4 vpMat; // View * Projection
	Mat4x4 mvpMat;
	ComPtr<ID3D12RootSignature> triangleRootSignature; // 三角形用ルートシグネチャ
	ComPtr<ID3D12RootSignature> textureRootSignature; // テクスチャ用ルートシグネチャ
	ComPtr<ID3D12RootSignature> cubeRootSignature; // キューブ用用ルートシグネチャ
	ComPtr<ID3D12PipelineState> trianglePipelineState; // 三角形用パイプラインステートオブジェクト
	ComPtr<ID3D12PipelineState> texturePipelineState; // テクスチャ用パイプラインステートオブジェクト
	ComPtr<ID3D12PipelineState> cubePipelineState; // キューブ用パイプラインステートオブジェクト
	ComPtr<ID3D12RootSignature> modelRootSignature; // モデル用ルートシグネチャ
	ComPtr<ID3D12PipelineState> modelPipeLineState; // モデル用パイプラインステート
	SpriteBatch fgBatch; // 手前のスプライトバッチ処理
	SpriteBatch bgBatch; // 背景のスプライトバッチ処理
	DebugTriangle triangle; // 三角形描画
	DebugQuad quad; // テクスチャ描画
	DebugCube cube; // キューブ描画
	Gfx::BitmapFont defaultFont; // デフォルト用の文字列
	int screenWidth = 0; // 画面の横幅
	int screenHeight = 0; // 画面の縦幅
}

// 初期化処理(これを呼ぶだけで初期化処理が済むようにする)
bool GfxInternal::Initialize(const wchar_t* _title, int _width, int _height)
{
	HRESULT result{};
	result = CoInitializeEx(nullptr, COINIT_MULTITHREADED); // COMを初期化
	DEBUG_ASSERT(SUCCEEDED(result));
	if (FAILED(result))
	{
		return false;
	}

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

	// シェーダーのコンパイル(今はいったん仮で固定)
	auto triangleVSBlob{ shaderSystem.Compile(L"../Src/Shaders/TriangleVS.hlsl", "main", "vs_5_0") }; // 三角形
	if (!triangleVSBlob)
	{
		DEBUG_LOG_ERROR("シェーダーファイル読み込みに失敗しました ファイル : {}\n", "../Src/Shaders/TriangleVS.hlsl");
		return false; // 読み込み失敗したらfalse
	}
	auto textureVSBlob = shaderSystem.Compile(L"../Src/Shaders/TextureVS.hlsl", "main", "vs_5_0"); // テクスチャ
	if (!textureVSBlob)
	{
		DEBUG_LOG_ERROR("シェーダーファイル読み込みに失敗しました ファイル : {}\n", "../Src/Shaders/TextureVS.hlsl");
		return false; // 読み込み失敗したらfalse
	}
	auto trianglePSBlob{ shaderSystem.Compile(L"../Src/Shaders/TrianglePS.hlsl", "main", "ps_5_0") }; // 三角形
	if (!trianglePSBlob)
	{
		DEBUG_LOG_ERROR("シェーダーファイル読み込みに失敗しました ファイル : {}\n", "../Src/Shaders/TrianglePS.hlsl");
		return false; // 読み込み失敗したらfalse
	}
	auto texturePSBlob = shaderSystem.Compile(L"../Src/Shaders/TexturePS.hlsl", "main", "ps_5_0"); // テクスチャ
	if (!texturePSBlob)
	{
		DEBUG_LOG_ERROR("シェーダーファイル読み込みに失敗しました ファイル : {}\n", ".. / Src / Shaders / TexturePS.hlsl");
		return false; // 読み込み失敗したらfalse
	}
	auto cubeVSBlob{ shaderSystem.Compile(L"../Src/Shaders/CubeVS.hlsl", "main", "vs_5_0") }; // キューブ
	if (!cubeVSBlob)
	{
		DEBUG_LOG_ERROR("シェーダーファイル読み込みに失敗しました ファイル : {}\n", "../Src/Shaders/CubeVS.hlsl");
		return false; // 読み込み失敗したらfalse
	}
	auto cubePSBlob{ shaderSystem.Compile(L"../Src/Shaders/CubePS.hlsl", "main", "ps_5_0") }; // キューブ
	if (!cubePSBlob)
	{
		DEBUG_LOG_ERROR("シェーダーファイル読み込みに失敗しました ファイル : {}\n", "../Src/Shaders/CubePS.hlsl");
		return false; // 読み込み失敗したらfalse
	}
	auto modelVSBlob{ shaderSystem.Compile(L"../Src/Shaders/ModelVS.hlsl", "main", "vs_5_0") }; // モデル
	if (!modelVSBlob)
	{
		DEBUG_LOG_ERROR("シェーダーファイル読み込みに失敗しました ファイル : {}\n", "../Src/Shaders/ModelVS.hlsl");
		return false; // 読み込み失敗したらfalse
	}
	auto modelPSBlob{ shaderSystem.Compile(L"../Src/Shaders/ModelPS.hlsl", "main", "ps_5_0") }; // モデル
	if (!modelPSBlob)
	{
		DEBUG_LOG_ERROR("シェーダーファイル読み込みに失敗しました ファイル : {}\n", "../Src/Shaders/ModelPS.hlsl");
		return false; // 読み込み失敗したらfalse
	}

	triangleRootSignature = shaderSystem.CreateDebugTriangleRootSignature(); // ルートシグネチャの作成
	if (!triangleRootSignature)
	{
		DEBUG_LOG_ERROR("三角形ルートシグネチャの作成に失敗しました\n");
		return false; // 作成失敗したらfalse

	}

	textureRootSignature = shaderSystem.CreateTextureRootSignature(); // ルートシグネチャの作成
	if (!textureRootSignature)
	{
		DEBUG_LOG_ERROR("テクスチャルートシグネチャの作成に失敗しました\n");
		return false;
	}

	cubeRootSignature = shaderSystem.CreateDebugCubeRootSignature(); // ルートシグネチャの作成
	if (!cubeRootSignature)
	{
		DEBUG_LOG_ERROR("Cubeシグネチャの作成に失敗しました\n");
		return false;
	}

	trianglePipelineState = shaderSystem.CreateDebugTriaglePipeLineState(triangleRootSignature.Get(), triangleVSBlob.Get(), trianglePSBlob.Get()); // パイプラインステートオブジェクトを作成
	if (!trianglePipelineState)
	{
		DEBUG_LOG_ERROR("三角形PSOの作成に失敗しました\n");
		return false;
	}


	texturePipelineState = shaderSystem.CreateTexturePipeLineState(textureRootSignature.Get(), textureVSBlob.Get(), texturePSBlob.Get()); // パイプラインステートオブジェクトを作成
	if (!texturePipelineState)
	{
		DEBUG_LOG_ERROR("テクスチャPSOの作成に失敗しました\n");
		return false;
	}

	cubePipelineState = shaderSystem.CreateDebugCubePipeLineState(cubeRootSignature.Get(), cubeVSBlob.Get(), cubePSBlob.Get()); // パイプラインステートオブジェクトを作成
	if (!cubePipelineState)
	{
		DEBUG_LOG_ERROR("テクスチャPSOの作成に失敗しました\n");
		return false;
	}

	modelRootSignature = shaderSystem.CreateModelRootSignature(); // モデルのルートシグネチャの作成
	if (!modelRootSignature)
	{
		DEBUG_LOG_ERROR("モデルルートシグネチャの作成に失敗しました\n");
		return false;
	}

	modelPipeLineState = shaderSystem.CreateModelPipeLineState(modelRootSignature.Get(), modelVSBlob.Get(), modelPSBlob.Get()); // モデルパイプラインステートの作成
	if (!modelPipeLineState)
	{
		DEBUG_LOG_ERROR("モデルPSOの作成に失敗しました\n");
		return false;
	}

	ResourceManager::Instance().Initialize(GraphicsDevice::Instance().GetDevice()); // リソース管理ファイルの初期化

	// ピクセル座標からNDC座標へ変換
	Mat4x4 orthMat{ Mat4x4::MakeOrthGraphic(static_cast<float>(_width), static_cast<float>(_height)) }; // 変換行列の作成
	orthConstantBufferData = ResourceManager::Instance().CreateConstantBuffer(&orthMat, sizeof(Mat4x4));

	// 透視投影行列の作成(一旦キューブが描画できるのを確認するためにハードコーディング)
	vpMat = Mat4x4::MakeLookAt({ 2.0f, 2.0f, -3.0f }, { 0.0f, 0.0f, 0.0f }, Vector3::Up) * Mat4x4::MakePerspective(60.0f * Math::DEG_TO_RAD, static_cast<float>(screenWidth) / static_cast<float>(screenHeight), 0.1f, 100.0f);
	mvpRingCBV.Initialize(sizeof(Mat4x4)); // リングバッファ初期化

	// スプライトバッチ処理初期化
	fgBatch.Initialize(textureRootSignature.Get(), texturePipelineState.Get(), orthConstantBufferData.resource.Get());
	bgBatch.Initialize(textureRootSignature.Get(), texturePipelineState.Get(), orthConstantBufferData.resource.Get());

	triangle.Initialize();  // 三角形描画用ファイルの初期化
	quad.Initialize(); // テクスチャ描画用ファイルの初期化
	cube.Initialize(); // キューブ初期化

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
	GraphicsDevice::Instance().EndFrame(); // フレームの最後の処理
}

// 終了処理
void GfxInternal::Finish()
{
	DescriptorManager::Instance().Shutdown();
	GraphicsDevice::Instance().Shutdown();
	CoUninitialize(); // COMも閉じる
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
	return ResourceManager::Instance().LoadTexture(_filePath);
}

// モデル読み込み
ModelHandle Gfx::LoadModel(const char* _filePath)
{
	return ResourceManager::Instance().LoadModel(_filePath);
}

// 三角形の描画(現状固定座標にしているが拡張し、座標と色など指定できるようにしたい)
void Gfx::DrawTriangle()
{
	// パイプライン設定
	GraphicsDevice::Instance().GetCommandList()->SetGraphicsRootSignature(triangleRootSignature.Get());
	GraphicsDevice::Instance().GetCommandList()->SetPipelineState(trianglePipelineState.Get());
	triangle.Draw(GraphicsDevice::Instance().GetCommandList());
}

void Gfx::DrawTexture()
{
	GraphicsDevice::Instance().GetCommandList()->SetGraphicsRootSignature(textureRootSignature.Get());
	GraphicsDevice::Instance().GetCommandList()->SetGraphicsRootConstantBufferView(1, orthConstantBufferData.resource->GetGPUVirtualAddress());
	GraphicsDevice::Instance().GetCommandList()->SetPipelineState(texturePipelineState.Get());
	quad.Draw(GraphicsDevice::Instance().GetCommandList());
}

void Gfx::DrawCube(Vector3 _angle)
{
	{
		// マクロがスコープを抜けるとEndEventするので囲う
		GPU_MARKER("backGround");
		bgBatch.Flush(); // 背景の上に来るように3D描画前には背景batchをFlushする
	}
	cube.SetRotation(_angle);
	mvpMat = cube.GetWorldMat() * vpMat; // mvp行列
	mvpRingCBV.Update(&mvpMat, sizeof(Mat4x4)); // 定数バッファの更新
	GraphicsDevice::Instance().GetCommandList()->SetGraphicsRootSignature(cubeRootSignature.Get());
	GraphicsDevice::Instance().GetCommandList()->SetGraphicsRootConstantBufferView(0, mvpRingCBV.GetCurrentVertualAddress());
	GraphicsDevice::Instance().GetCommandList()->SetPipelineState(cubePipelineState.Get());
	cube.Draw(GraphicsDevice::Instance().GetCommandList());
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

void Gfx::DrawModel(ModelHandle _model, Transform _transform)
{
	ModelData* model{ ResourceManager::Instance().Lookup(_model) };
	if (!model) return; // 無効ハンドルガード
	auto cmd{ GraphicsDevice::Instance().GetCommandList() }; // コマンドリストのキャッシュ
	Mat4x4 worldMat{ _transform.GetWorldMatrix() };
	mvpMat = worldMat * vpMat;
	mvpRingCBV.Update(&mvpMat, sizeof(Mat4x4)); // リングバッファ更新

	// パイプライン設定
	cmd->SetGraphicsRootSignature(modelRootSignature.Get());
	cmd->SetPipelineState(modelPipeLineState.Get());

	// SRVヒープをバインド(テクスチャを使うため)
	DescriptorManager::Instance().SetDiscriptor(cmd); // Flushと同じ考え方
	cmd->SetGraphicsRootConstantBufferView(0, mvpRingCBV.GetCurrentVertualAddress());
	cmd->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_LINELIST);

	// submeshループ
	for (const SubMesh& sub : model->subMeshes)
	{
		// テクスチャをバインド
		TextureData* tex{ ResourceManager::Instance().Lookup(sub.texture) };
		if (tex) cmd->SetGraphicsRootDescriptorTable(1, tex->srvHandle.gpu);

		// 頂点インデックスをバインド
		cmd->IASetVertexBuffers(0, 1, &sub.vertexBuffer.vertexView);
		cmd->IASetIndexBuffer(&sub.indexBuffer.indexView);

		cmd->DrawIndexedInstanced(sub.indexBuffer.indexCount, 1, 0, 0, 0);
	}
}


// 解放
void Gfx::Unload(TexHandle _handle)
{
	ResourceManager::Instance().Unload(_handle);
}

void Gfx::Unload(ModelHandle _handle)
{
	ResourceManager::Instance().Unload(_handle);
}