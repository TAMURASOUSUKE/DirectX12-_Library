#include "../Src/Facade/Gfx.h"
#include "GraphicsDevice.h" // デバイス用のテスト
#include "DescriptorManager.h" // Allocator関数を呼び出しメモリ確保できるかのテスト

// エントリーポイント
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{

	// 初期化 失敗したら-1を返す
	if (!Gfx::Initialize(L"GraphicsTest", 1280, 720)) return -1;


	DescriptorHandle h1{ DescriptorManager::Instance().Allocate(HeapType::CBV_SRV_UAV) }; // GPU可視
	DescriptorHandle h2{ DescriptorManager::Instance().Allocate(HeapType::CBV_SRV_UAV) }; // GPU可視

	// h1とh2が別のインデックスであることを確認する
	if (h1.index != h2.index)
	{
		OutputDebugStringA("[PASS] : インデックスが異なる値を出力できています");
	}
	else
	{
		OutputDebugStringA("[FAIL] : インデックスが同じ値を出力しています");
	}

	// Freeして再度Allocateすると同じインデックスが戻るか
	DescriptorManager::Instance().Free(HeapType::CBV_SRV_UAV, h2);
	DescriptorHandle h3{ DescriptorManager::Instance().Allocate(HeapType::CBV_SRV_UAV) }; // 再度取得

	if (h2.index == h3.index)
	{
		OutputDebugStringA("[PASS] : 一度戻した後も同じインデックスが返っています");
	}
	else
	{
		OutputDebugStringA("[FAIL] : 一度戻した後違うインデックスが返っています");
	}

	while (Gfx::ProcessMessage())
	{
		Gfx::BeginFrame(); // フレーム開始処理
		Gfx::ClearScreen(); // 画面クリア(黒)

		Gfx::DrawTriangle(); // 三角形描画
		Gfx::DrawTexture(); // テクスチャ描画

		Gfx::EndFrame(); // フレーム終了処理
	}
	Gfx::Finish(); // 終了
	return 0;
}