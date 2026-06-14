#include "../Window/Window.h"
#include "../Graphics/GraphicsDevice.h"
#include "../Graphics/DescriptorManager.h"
#include "../Graphics/ShaderSystem.h"
#include "../Graphics/ResourceManager.h"
#include "../Graphics/SpriteBatch.h"
#include "../Graphics/RingConstantBuffer.h"
#include "../Graphics/DrawDebug/DebugTriangle.h"
#include "../Graphics/DrawDebug/DebugQuad.h"
#include "../Graphics/DrawDebug/DebugCube.h"
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
	SpriteBatch spriteBatch; // スプライトバッチ処理
	DebugTriangle triangle; // 三角形描画
	DebugQuad quad; // テクスチャ描画
	DebugCube cube; // キューブ描画
	int screenWidth = 0; // 画面の横幅
	int screenHeight = 0; // 画面の縦幅
}

// 初期化処理(これを呼ぶだけで初期化処理が済むようにする)
bool GfxInternal::Initialize(const wchar_t* _title, int _width, int _height)
{
	screenWidth = _width;
	screenHeight = _height;

	window.SetWindowName(_title); // 名前設定
	window.GenerateWindow(); // ウィンドウを作成
	if (!window.GetHWND()) return false; // ウィンドウ作成失敗ならfalse

	GraphicsDevice::Instance().Initialize(window.GetHWND(), _width, _height); // デバイスの初期化
	if (!GraphicsDevice::Instance().GetDevice()) return false; // デバイス読み込み失敗したらfalse

	DescriptorManager::Instance().Initialize(GraphicsDevice::Instance().GetDevice()); // ディスクリプタマネージャーをデバイスを使って初期化

	shaderSystem.Initialize(GraphicsDevice::Instance().GetDevice()); // ShaderSystemの初期化

	// シェーダーのコンパイル(今はいったん仮で固定)
	auto triangleVSBlob{ shaderSystem.Compile(L"../Src/Shaders/TriangleVS.hlsl", "main", "vs_5_0") }; // 三角形
	if (!triangleVSBlob) return false; // 読み込み失敗したらfalse
	auto textureVSBlob = shaderSystem.Compile(L"../Src/Shaders/TextureVS.hlsl", "main", "vs_5_0"); // テクスチャ
	if (!textureVSBlob) return false; // 読み込み失敗したらfalse
	auto trianglePSBlob{ shaderSystem.Compile(L"../Src/Shaders/TrianglePS.hlsl", "main", "ps_5_0") }; // 三角形
	if (!trianglePSBlob) return false; // 読み込み失敗したらfalse
	auto texturePSBlob = shaderSystem.Compile(L"../Src/Shaders/TexturePS.hlsl", "main", "ps_5_0"); // テクスチャ
	if (!texturePSBlob) return false; // 読み込み失敗したらfalse
	auto cubeVSBlob{ shaderSystem.Compile(L"../Src/Shaders/CubeVS.hlsl", "main", "vs_5_0") }; // キューブ
	if (!cubeVSBlob) return false; // 読み込み失敗したらfalse
	auto cubePSBlob{ shaderSystem.Compile(L"../Src/Shaders/CubePS.hlsl", "main", "ps_5_0") }; // キューブ
	if (!cubePSBlob) return false; // 読み込み失敗したらfalse

	triangleRootSignature =  shaderSystem.CreateDebugTriangleRootSignature(); // ルートシグネチャの作成
	if (!triangleRootSignature) return false; // 読み込み失敗したらfalse

	textureRootSignature = shaderSystem.CreateDebugTextureRootSignature(); // ルートシグネチャの作成
	if (!textureRootSignature) return false;

	cubeRootSignature = shaderSystem.CreateDebugCubeRootSignature(); // ルートシグネチャの作成
	if (!cubeRootSignature) return false;

	trianglePipelineState = shaderSystem.CreateDebugTriaglePipeLineState(triangleRootSignature.Get(), triangleVSBlob.Get(), trianglePSBlob.Get()); // パイプラインステートオブジェクトを作成
	if (!trianglePipelineState) return false;

	texturePipelineState = shaderSystem.CreateDebugTexturePipeLineState(textureRootSignature.Get(), textureVSBlob.Get(), texturePSBlob.Get()); // パイプラインステートオブジェクトを作成
	if (!texturePipelineState) return false;

	cubePipelineState = shaderSystem.CreateDebugCubePipeLineState(cubeRootSignature.Get(), cubeVSBlob.Get(), cubePSBlob.Get()); // パイプラインステートオブジェクトを作成
	if (!cubePipelineState) return false;

	ResourceManager::Instance().Initialize(GraphicsDevice::Instance().GetDevice()); // リソース管理ファイルの初期化

	// ピクセル座標からNDC座標へ変換
	Mat4x4 orthMat{ Mat4x4::MakeOrthGraphic(static_cast<float>(_width), static_cast<float>(_height)) }; // 変換行列の作成
	orthConstantBufferData = ResourceManager::Instance().CreateConstantBuffer(&orthMat, sizeof(Mat4x4));

	// 透視投影行列の作成(一旦キューブが描画できるのを確認するためにハードコーディング)
	vpMat = Mat4x4::MakeLookAt({ 2.0f, 2.0f, -3.0f }, { 0.0f, 0.0f, 0.0f }, Vector3::Up) * Mat4x4::MakePerspective(60.0f * Math::DEG_TO_RAD, static_cast<float>(screenWidth) / static_cast<float>(screenHeight), 0.1f, 100.0f);
	mvpRingCBV.Initialize(sizeof(Mat4x4)); // リングバッファ初期化

	// スプライトバッチ処理初期化
	spriteBatch.Initialize(textureRootSignature.Get(), texturePipelineState.Get(), orthConstantBufferData.resource.Get());

	triangle.Initialize();  // 三角形描画用ファイルの初期化
	quad.Initialize(); // テクスチャ描画用ファイルの初期化
	cube.Initialize(); // キューブ初期化
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

	spriteBatch.Reset(); // カウンターリセット

	auto cmdList{GraphicsDevice::Instance().GetCommandList()}; // コマンドリスト
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
	spriteBatch.Flush(); // Spritebatch描画
	GraphicsDevice::Instance().EndFrame(); // フレームの最後の処理
}

// 終了処理
void GfxInternal::Finish()
{
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
	return ResourceManager::Instance().LoadTexture(_filePath);
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
	cube.SetRotation(_angle);
	mvpMat = cube.GetWorldMat() * vpMat; // mvp行列
	mvpRingCBV.Update(&mvpMat, sizeof(Mat4x4)); // 定数バッファの更新
	GraphicsDevice::Instance().GetCommandList()->SetGraphicsRootSignature(cubeRootSignature.Get());
	GraphicsDevice::Instance().GetCommandList()->SetGraphicsRootConstantBufferView(0, mvpRingCBV.GetCurrentVertualAddress());
	GraphicsDevice::Instance().GetCommandList()->SetPipelineState(cubePipelineState.Get());
	cube.Draw(GraphicsDevice::Instance().GetCommandList());
}

// 画像登録
void Gfx::DrawSprite(TexHandle _texture, Vector2 _position, Vector2 _size, float _radRotation)
{
	spriteBatch.RegisterSprite(_texture, _position, _size, _radRotation);
}

// 解放
void Gfx::Unload(TexHandle _handle)
{
	ResourceManager::Instance().Unload(_handle);
}