#include "../Src/Facade/TSLib.h"
#include <string> // テスト用
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

	// ハンドルの取得
	TexHandle background{ Gfx::LoadTexture("Res/bg.png") }; // 背景のハンドル取得
	TexHandle enemy{ Gfx::LoadTexture("Res/enemy.png") }; // Enemyのハンドル取得
	TexHandle player{ Gfx::LoadTexture("Res/player.png") }; // Playerのハンドル取得
	ModelHandle testModel{ Gfx::LoadModel("Res/TestMultipleAnimModel.glb") }; // Playerモデルのロード
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

	while (Gfx::ProcessMessage())
	{
		TSLib::BeginFrame(); // フレーム開始処理

		Vector2 dir{ Input::GetPadStickValue(PadCode::Stick::RIGHT)};

		playerPos += dir * 8.0f;

		ang += 0.01f;

		ang = Math::NormalizeAngle(ang); // 角度の正規化

		cubeAng.x += 0.01f;
		cubeAng.y += 0.01f;
		cubeAng.z += 0.01f;
		cubeAng = Math::NormalizeAngle(cubeAng);

		// アニメーションテスト
		debugAnim.currentTime += 1.0f / 60.0f; // 時刻を進める
		if (debugAnim.currentTime > 0.667f) debugAnim.currentTime = 0.0f; // 一旦Runのdurationでループさせる

		Gfx::ClearScreen(); // 画面クリア(黒)teku

		//// スプライトバッチテスト
		Gfx::DrawSprite(background, Vector2{ 0.0f, 0.0f }, Vector2{ 1280.0f, 720.0f }, 0.0f, Vector2::Zero, Vector2::One, LenderLayer::BackGround);
		Gfx::DrawSprite(player, playerPos, Vector2{128.0f, 128.0f});

		TexHandle test{};
		if (Input::IsPadReleased(PadCode::Trigger::RIGHT) || Input::IsKeyPushed(KeyCode::Button::D)) testFlag = !testFlag;
		if (testFlag)
		{
			test = player;
		}
		else
		{
			test = enemy;
		}

		Gfx::DrawSprite(test, Vector2{ 800.0f, 400.0f }, Vector2{ 128.0f, 128.0f });

		float inputTrigger{ Input::GetPadTriggerValue(PadCode::Trigger::RIGHT) };
		std::string triggerStr{ std::format("Input Trigger Value : {:.2f}", inputTrigger) };
		Gfx::DrawString(triggerStr.c_str(), {0.0f, 0.0f});

		Gfx::DrawModel(testModel, cubeTransform, &debugAnim);

		Gfx::DrawCapsule({30.0f, 30.0f}, {30.0f, 200.0f}, 40.0f, {1.0f, 1.0f, 1.0f, 1.0f}, true);
		Gfx::DrawCapsule({120.0f, 80.0f}, {120.0f, 200.0f}, 40.0f, { 0.0f, 1.0f, 0.0f, 1.0f }, true);

		//Gfx::DrawLine({ 300.0f, 300.0f }, { 700.0f, 20.0f });
		//Gfx::DrawLine({ 300.0f, 300.0f }, { 1000.0f, 1000.0f }, {0.3f, 0.75f, 0.87f, 1.0f});

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