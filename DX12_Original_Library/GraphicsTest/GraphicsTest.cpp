#include "../Src/Facade/TSLib.h"
#include "DescriptorManager.h" // Allocator関数を呼び出しメモリ確保できるかのテスト

// エントリーポイント
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{

	// 初期化 失敗したら-1を返す
	if (!TSLib::Initialize(L"GraphicsTest", 1280, 720)) return -1;


	DescriptorHandle h1{ DescriptorManager::Instance().Allocate(HeapType::CBV_SRV_UAV) }; // GPU可視
	DescriptorHandle h2{ DescriptorManager::Instance().Allocate(HeapType::CBV_SRV_UAV) }; // GPU可視

	// h1とh2が別のインデックスであることを確認する
	if (h1.index != h2.index)
	{
		OutputDebugStringA("[PASS] : インデックスが異なる値を出力できています\n");
	}
	else
	{
		OutputDebugStringA("[FAIL] : インデックスが同じ値を出力しています\n");
	}

	// Freeして再度Allocateすると同じインデックスが戻るか
	DescriptorManager::Instance().Free(HeapType::CBV_SRV_UAV, h2);
	DescriptorHandle h3{ DescriptorManager::Instance().Allocate(HeapType::CBV_SRV_UAV) }; // 再度取得

	if (h2.index == h3.index)
	{
		OutputDebugStringA("[PASS] : 一度戻した後も同じインデックスが返っています\n");
	}
	else
	{
		OutputDebugStringA("[FAIL] : 一度戻した後違うインデックスが返っています\n");
	}

	// ハンドルの取得
	TexHandle background{ Gfx::LoadTexture("Res/bg.png") }; // 背景のハンドル取得
	TexHandle enemy{ Gfx::LoadTexture("Res/enemy.png") }; // Enemyのハンドル取得
	TexHandle player{ Gfx::LoadTexture("Res/player.png") }; // Playerのハンドル取得
	ModelHandle testModel{ Gfx::LoadModel("Res/TestPlayer.glb") }; // Playerモデルのロード
	Vector2 playerPos{ 100.0f, 100.0f };
	float ang{ 0.0f }; // 角度加算用のテスト
	Vector3 cubeAng{ Vector3::Zero };

	Transform cubeTransform{};
	cubeTransform.SetPosition(Vector3{ 0.0f, 0.0f, 0.0f });
	cubeTransform.SetScale(Vector3::One);

	while (Gfx::ProcessMessage())
	{
		TSLib::BeginFrame(); // フレーム開始処理

		Vector2 dir{ Vector2::Zero };
		if (Input::IsKeyPress(KeyCode::D)) dir.x += 1.0f;
		if (Input::IsKeyPress(KeyCode::A)) dir.x -= 1.0f;
		if (Input::IsKeyPress(KeyCode::W)) dir.y -= 1.0f;
		if (Input::IsKeyPress(KeyCode::S)) dir.y += 1.0f;

		dir.Normalize(); // 正規化

		playerPos += dir * 8.0f;

		ang += 0.01f;

		ang = Math::NormalizeAngle(ang); // 角度の正規化

		cubeAng.x += 0.01f;
		cubeAng.y += 0.01f;
		cubeAng.z += 0.01f;
		cubeAng = Math::NormalizeAngle(cubeAng);

		Gfx::ClearScreen(); // 画面クリア(黒)

		//// スプライトバッチテスト
		Gfx::DrawSprite(background, Vector2{ 0.0f, 0.0f }, Vector2{ 1280.0f, 720.0f }, 0.0f, Vector2::Zero, Vector2::One, LenderLayer::BackGround);
		//for (int i = 0; i < 400; i++)
		//{
		//	float offset{ i * 10.0f };
		//	Gfx::DrawSprite(player, Vector2{ playerPos.x + offset, playerPos.y + 100.0f}, Vector2{128.0f, 128.0f}, ang);
		//	Gfx::DrawSprite(player, Vector2{ playerPos.x + offset, playerPos.y + 200.0f }, Vector2{ 128.0f, 128.0f }, ang);
		//	Gfx::DrawSprite(player, Vector2{ playerPos.x + offset, playerPos.y + 300.0f }, Vector2{ 128.0f, 128.0f }, ang);
		//	Gfx::DrawSprite(player, Vector2{ playerPos.x + offset, playerPos.y + 400.0f }, Vector2{ 128.0f, 128.0f }, ang);
		//	Gfx::DrawSprite(player, Vector2{ playerPos.x + offset, playerPos.y + 500.0f }, Vector2{ 128.0f, 128.0f }, ang);
		//}
		Gfx::DrawSprite(enemy, Vector2{ 800.0f, 400.0f }, Vector2{ 128.0f, 128.0f });

		Gfx::DrawString("MeshNum : ", {0.0f, 0.0f});

		// Gfx::DrawCube(cubeAng);

		Gfx::DrawModel(testModel, cubeTransform);

		TSLib::EndFrame(); // フレーム終了処理
	}

	// 解放
	Gfx::Unload(background);
	Gfx::Unload(enemy);
	Gfx::Unload(player);
	Gfx::Unload(testModel);

	TSLib::Finish(); // 終了
	return 0;
}