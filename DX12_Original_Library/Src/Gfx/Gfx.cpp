#include "Window.h"
#include "../Graphics/GraphicsDevice.h"
#include "../Graphics/DescriptorManager.h"
#include "../Graphics/ShaderSystem.h"
#include "../Graphics/ResourceManager.h"
#include "../Graphics/DrawDebug/DebugTriangle.h"
#include "../Graphics/DrawDebug/DebugQuad.h"
#include "Gfx.h"

// 無名名前空間で変数を保持する
namespace {
	Window window; // window作成クラス
	ShaderSystem shaderSystem; // Shader読み込みなどを管理するファイル
	ComPtr<ID3D12RootSignature> triangleRootSignature; // ルートシグネチャ
	ComPtr<ID3D12RootSignature> textureRootSignature; // ルートシグネチャ
	ComPtr<ID3D12PipelineState> trianglePipelineState; // パイプラインステートオブジェクト
	ComPtr<ID3D12PipelineState> texturePipelineState; // パイプラインステートオブジェクト
	DebugTriangle triangle; // 三角形描画
	DebugQuad quad; // テクスチャ描画
	int screenWidth = 0; // 画面の横幅
	int screenHeight = 0; // 画面の縦幅
}

// 初期化処理(これを呼ぶだけで初期化処理が済むようにする)
bool Gfx::Initialize(const wchar_t* _title, int _width, int _height)
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
	auto triangleVsBlob{ shaderSystem.Compile(L"../Src/Shaders/TriangleVS.hlsl", "main", "vs_5_0") }; // 三角形
	if (!triangleVsBlob) return false; // 読み込み失敗したらfalse
	auto textureVSBlob = shaderSystem.Compile(L"../Src/Shaders/TextureVS.hlsl", "main", "vs_5_0"); // テクスチャ
	if (!textureVSBlob) return false; // 読み込み失敗したらfalse
	auto trianglePsBlob{ shaderSystem.Compile(L"../Src/Shaders/TrianglePS.hlsl", "main", "ps_5_0") }; // 三角形
	if (!trianglePsBlob) return false; // 読み込み失敗したらfalse
	auto texturePSBlob = shaderSystem.Compile(L"../Src/Shaders/TexturePS.hlsl", "main", "ps_5_0"); // テクスチャ
	if (!texturePSBlob) return false; // 読み込み失敗したらfalse

	triangleRootSignature =  shaderSystem.CreateDebugTriangleRootSignature(); // ルートシグネチャの作成
	if (!triangleRootSignature) return false; // 読み込み失敗したらfalse

	textureRootSignature = shaderSystem.CreateDebugTextureRootSignature(); // ルートシグネチャの作成
	if (!textureRootSignature) return false;

	trianglePipelineState = shaderSystem.CreateDebugTriaglePipeLineState(triangleRootSignature.Get(), triangleVsBlob.Get(), trianglePsBlob.Get()); // パイプラインステートオブジェクトを作成
	if (!trianglePipelineState) return false;

	texturePipelineState = shaderSystem.CreateDebugTexturePipeLineState(textureRootSignature.Get(), textureVSBlob.Get(), texturePSBlob.Get()); // パイプラインステートオブジェクトを作成
	if (!texturePipelineState) return false;

	ResourceManager::Instance().Initialize(GraphicsDevice::Instance().GetDevice()); // リソース管理ファイルの初期化
	triangle.Initialize();  // 三角形描画用ファイルの初期化
	quad.Initialize(); // テクスチャ描画用ファイルの初期化
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
void Gfx::BeginFrame()
{
	GraphicsDevice::Instance().BeginFrame(); // フレームの最初の処理

	auto cmdList{GraphicsDevice::Instance().GetCommandList()}; // コマンドリスト
	auto rtv{ GraphicsDevice::Instance().GetCurrentRTV() }; // 現在のRTV

	// レンダーターゲット設定
	cmdList->OMSetRenderTargets(1, &rtv, false, nullptr);

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
void Gfx::EndFrame()
{
	GraphicsDevice::Instance().EndFrame(); // フレームの最後の処理
}

// 終了処理
void Gfx::Finish()
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
	GraphicsDevice::Instance().GetCommandList()->SetPipelineState(texturePipelineState.Get());
	quad.Draw(GraphicsDevice::Instance().GetCommandList());
}

