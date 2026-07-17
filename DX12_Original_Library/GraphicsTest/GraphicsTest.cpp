#include "../Src/Facade/TSLib.h"
#include <string> // テスト用
#include <algorithm>
#include "../Src/Graphics/GraphicsType.h" // デバッグ用に一時的に
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

	// 掃除
	DescriptorManager::Instance().Free(HeapType::CBV_SRV_UAV, h1);
	DescriptorManager::Instance().Free(HeapType::CBV_SRV_UAV, h3);

	// ハンドルの取得
	TexHandle background{ Gfx::LoadTexture("Res/bg.png") }; // 背景のハンドル取得
	TexHandle enemy{ Gfx::LoadTexture("Res/enemy.png") }; // Enemyのハンドル取得
	TexHandle player{ Gfx::LoadTexture("Res/player.png") }; // Playerのハンドル取得
	TexHandle heightMap{ Gfx::LoadTexture("Res/TestVolume.png") }; // ハイトマップ取得
	// TexHandle heightMap{ Gfx::LoadTexture("Res/Crater.jpg") }; // ハイトマップ取得
	ModelHandle testModel{ Gfx::LoadModel("Res/TestMultipleAnimModel.glb") }; // Testモデルのロード
	ModelHandle testPlayer{ Gfx::LoadModel("Res/TestPlayer.glb") }; // Playerモデルのロード
	AnimInstanceData debugAnim{}; // アニメーション用のデータ
	debugAnim.handle = testModel;
	Vector2 playerPos{ 100.0f, 100.0f };
	float ang{ 0.0f }; // 角度加算用のテスト
	Vector3 cubeAng{ Vector3::Zero };

	Transform cubeTransform{};
	Transform cubeTransform02{};
	cubeTransform.SetPosition(Vector3{ 0.0f, 0.0f, 0.0f });
	cubeTransform02.SetPosition(Vector3{ 0.0f, 0.0f, 0.0f });
	cubeTransform.SetScale(Vector3::One);

	bool testFlag{ false };
	int testWheel{ 0 };
	int testWheelNotch{ 0 };

	float t{ 0.0f }; // 時間
	float tessFactor{ 4.0f }; // HSでの分割数
	float heightFactor{ 0.0f }; // Terrainの高さ

	Rect testRect01{ {200.0f, 200.0f}, {30.0f, 30.0f} };
	Rect testRect02{ {400.0f, 400.0f}, {30.0f, 30.0f} };
	Vector4 debugColor{ 1.0f, 1.0f, 1.0f, 1.0f };

	while (TSLib::ProcessMessage())
	{
		TSLib::BeginFrame(); // フレーム開始処理

		t += 0.0167f;

		ang += 0.01f;

		ang = Math::NormalizeAngle(ang); // 角度の正規化

		cubeAng.x += 0.01f;
		cubeAng.y += 0.01f;
		cubeAng.z += 0.01f;
		cubeAng = Math::NormalizeAngle(cubeAng);

		// アニメーションテスト
		debugAnim.currentTime += 1.0f / 60.0f; // 時刻を進める
		if (debugAnim.currentTime > 0.667f) debugAnim.currentTime = 0.0f; // 一旦Runのdurationでループさせる

		// Terrain操作
		if (Input::IsKeyPress(KeyCode::Button::RIGHT)) heightFactor += 0.05f;
		if (Input::IsKeyPress(KeyCode::Button::LEFT)) heightFactor -= 0.05f;
		if(Input::IsKeyPushed(KeyCode::Button::L)) tessFactor *= 2.0f;
		if (Input::IsKeyPushed(KeyCode::Button::J)) tessFactor /= 2.0f;
		if (tessFactor < 2.0f) tessFactor = 2.0f;

		Vector2 dir{ Vector2::Zero };
		if (Input::IsKeyPress(KeyCode::Button::W)) dir.y -= 1.0f;
		if (Input::IsKeyPress(KeyCode::Button::A)) dir.x -= 1.0f;
		if (Input::IsKeyPress(KeyCode::Button::S)) dir.y += 1.0f;
		if (Input::IsKeyPress(KeyCode::Button::D)) dir.x += 1.0f;
		dir.Normalize();

		testRect01.position += dir * 8.0f;

		if (Collision::Intersect(testRect01, testRect02))
		{
			debugColor = { 1.0f, 0.0f, 0.0f, 1.0f };
		}
		else
		{
			debugColor = { 1.0f, 1.0f, 1.0f, 1.0f };
		}

		Gfx::ClearScreen(); // 画面クリア(黒)

		//// スプライトバッチテスト
		Gfx::DrawSprite(background, { 0.0f, 0.0f }, { 1280.0f, 720.0f }, 0.0f, Vector2::Zero, Vector2::One, LenderLayer::BackGround);
		Gfx::DrawSprite(enemy, { 350.0f, 350.0f }, { 128.0f, 128.0f });

		Gfx::DrawTerrain({ 0.0f, -10.0f, 20.0f }, 80.0f, tessFactor, heightFactor, { 1.0f, 0.0f, 0.0f, 0.0f }, heightMap);

		Gfx::DrawModel(testModel, cubeTransform, &debugAnim);

		Gfx::DrawSprite(enemy, {500.0f, 500.0f}, { 128.0f, 128.0f });

		//Gfx::DrawCapsule({30.0f, 30.0f}, {30.0f, 200.0f}, 40.0f, {1.0f, 1.0f, 1.0f, 1.0f}, true);
		//Gfx::DrawCapsule({120.0f, 80.0f}, {120.0f, 200.0f}, 40.0f, { 0.0f, 1.0f, 0.0f, 1.0f }, true);
		//Gfx::DrawCircle({ 120.0f, 300.0f }, 30.0f, { 1.0f, 0.0f, 1.0f, 1.0f });
		//Gfx::DrawCircle({ std::sinf(t) * 50.0f + 600.0f, 300.0f}, 30.0f, {std::clamp(std::sinf(t), 0.0f, 1.0f), 0.0f, 0.0f, 1.0f});

		Gfx::DrawBox(testRect01.GetMinPos(), testRect01.GetMaxPos());
		Gfx::DrawBox(testRect02.GetMinPos(), testRect02.GetMaxPos(), 0.0f, debugColor);

		TSLib::EndFrame(); // フレーム終了処理
	}

	TSLib::Finish(); // 終了
	return 0;
}