#include "../Src/Facade/TSLib.h"
#include <string> // テスト用
#include <algorithm>

// エントリーポイント
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	const Vector2 windowSize{ 1280.0f, 720.0f };

	// 初期化 失敗したら-1を返す
	if (!TSLib::Initialize(L"GraphicsTest3D", static_cast<int>(windowSize.x), static_cast<int>(windowSize.y)))return -1;
	Time::SetTargetFPS(0);

	TexHandle heightMap{ Gfx::LoadTexture("Res/TestVolume.png") }; // ハイトマップ取得

	// Model
	ModelHandle testModel{ Gfx::LoadModel("Res/TestMultipleAnimModel.glb") }; // Testモデルのロード
	ModelHandle testPlayer{ Gfx::LoadModel("Res/TestPlayer.glb") }; // Playerモデルのロード
	Transform modelTransform01{};
	Transform modelTransform02{};
	modelTransform01.SetPosition({ -1.0f, 0.0f, 0.0f });
	modelTransform02.SetPosition({ 1.0f, 0.0f, 0.0f });
	AnimInstanceHandle testModelAnim01{ Gfx::CreateAnimInstance(testModel) }; // testModelからAnimationのInstanceを作る

	float t{ 0.0f }; // 時間
	float tessFactor{ 4.0f }; // HSでの分割数
	float heightFactor{ 0.0f }; // Terrainの高さ

	// タイムスケール
	constexpr float ANIM_FRAME_DURATION{ 1.0f / 10.0f };
	float timeScale{ 1.0f };
	float time{ 0.0f };
	int spriteAnimTime{ 0 };

	// 3D基礎図形
	Transform cube0{};
	cube0.SetPosition({ -2.5f, 1.0f, 3.0f });
	Transform sphere{};
	sphere.SetPosition({ -1.5f, 1.0f, 3.0f });
	Transform cylinder{};
	cylinder.SetPosition({ -1.0f, 1.0f, 3.0f });
	Transform plane{};
	plane.SetPosition({ 0.0f, -2.0f, 4.0f });

	// ゲームループ
	while (TSLib::ProcessMessage() && !Input::IsKeyPushed(KeyCode::Button::ESC))
	{
		TSLib::BeginFrame(); // フレーム開始処理
		time += Time::DeltaTime();


		// Terrain操作
		float heightSpeed{ 3.0f };
		if (Input::IsKeyPress(KeyCode::Button::UP)) heightFactor += heightSpeed * Time::UnscaledDeltaTime();
		if (Input::IsKeyPress(KeyCode::Button::DOWN)) heightFactor -= heightSpeed * Time::UnscaledDeltaTime();
		if (Input::IsKeyPushed(KeyCode::Button::D2)) tessFactor *= 2.0f;
		if (Input::IsKeyPushed(KeyCode::Button::D1)) tessFactor /= 2.0f;
		tessFactor = std::clamp(tessFactor, 2.0f, 64.0f);

		heightFactor = std::clamp(heightFactor, -20.0f, 20.0f);

		// FPS操作
		if (Input::IsKeyPushed(KeyCode::Button::D3)) Time::SetTargetFPS(30); // 30FPS
		if (Input::IsKeyPushed(KeyCode::Button::D6)) Time::SetTargetFPS(60); // 60FPS
		if (Input::IsKeyPushed(KeyCode::Button::D0)) Time::SetTargetFPS(120); // 120FPS モニターが120Hz以上である必要あり
		if (Input::IsKeyPushed(KeyCode::Button::RIGHT)) timeScale += 1.0f;
		if (Input::IsKeyPushed(KeyCode::Button::LEFT)) timeScale -= 1.0f;
		timeScale = std::clamp(timeScale, 0.0f, 10.0f); // 最大でもタイムスケールは10にとどめておく
		Time::SetTimeScale(timeScale);

		// 3Dモデルアニメーション
		// 通常ループ再生
		if (Input::IsKeyPushed(KeyCode::Button::SPACE)) Gfx::PlayAnim(testModelAnim01, 0, true, 1.0f);
		// 逆方向ループ再生
		if (Input::IsKeyPushed(KeyCode::Button::R)) Gfx::PlayAnim(testModelAnim01, 0, true, -1.0f);
		// 現在の姿勢で一時停止
		if (Input::IsKeyPushed(KeyCode::Button::P)) Gfx::PauseAnim(testModelAnim01);
		// 一時停止した位置から再開
		if (Input::IsKeyPushed(KeyCode::Button::O)) Gfx::ResumeAnim(testModelAnim01);
		// 再生方向に応じた開始位置へ戻して停止
		if (Input::IsKeyPushed(KeyCode::Button::S)) Gfx::StopAnim(testModelAnim01);
		Gfx::UpdateAnim(testModelAnim01, Time::DeltaTime());

		std::string fpsValue{ std::format("CurrentMeasuredFPS : {:.1f}", Time::FPS()) };
		std::string targetFPS{ std::format("CurrentSettingFPS : {}", Time::GetTargetFPS()) };
		std::string unscaledDeltaTime{ std::format("CurrentUnscaledDeltaTime: {:.6f}", Time::UnscaledDeltaTime()) };
		std::string deltaTime{ std::format("CurrentDeltaTime : {:.3f}", Time::DeltaTime()) };
		std::string timeScale{ std::format("CurrentTimeScale : {:.2f}", Time::GetTimeScale()) };

		Gfx::ClearScreen(); // 画面クリア(黒)


		// 3Dモデルアニメーション
		Gfx::DrawAnimatedModel(testModelAnim01, modelTransform01);

		// 3D基礎図形
		Gfx::DrawCube3D(cube0, { 1.0f, 0.0f, 1.0f, 1.0f }, Gfx::Primitive3DStyle::DebugLine);
		Gfx::DrawSphere3D(sphere, { 0.0f, 0.0f, 0.0f, 1.0f }, Gfx::Primitive3DStyle::Fill);
		Gfx::DrawCylinder3D(cylinder, { 0.0f, 1.0f, 0.0f, 1.0f }, Gfx::Primitive3DStyle::DebugLine);
		Gfx::DrawCapsule3D({ 0.0f, -2.0f, 5.0f }, { 0.0f,  0.0f, 5.0f }, 0.5f, { 0.2f, 1.0f, 0.3f }, Gfx::Primitive3DStyle::Fill);
		Gfx::DrawPlane3D(plane, { 0.0f, 0.3f, 0.4f }, Gfx::Primitive3DStyle::Fill);
		Gfx::DrawLine3D({ 2.0f, 1.0f, 6.0f }, {-1.0f, -3.0f, 6.0f});
		// Gfx::DrawGrid3D({ 0.0f, -1.0f, 5.0f }, Quaternion::FromEuler(-45.0f * Math::DEG_TO_RAD, 0.0f, 0.0f), 10, 1.0f, {0.4f, 0.4f, 0.4f, 1.0f});
		Gfx::DrawWorldAxisGrid({0.0f, -1.0f, 5.0f});

		Gfx::DrawString(fpsValue.c_str(), { 0.0f, 0.0f });
		Gfx::DrawString(targetFPS.c_str(), { 0.0f, 30.0f });
		Gfx::DrawString(unscaledDeltaTime.c_str(), { 0.0f, 60.0f });
		Gfx::DrawString(deltaTime.c_str(), { 0.0f, 90.0f });
		Gfx::DrawString(timeScale.c_str(), { 0.0f, 120.0f });

		TSLib::EndFrame(); // フレーム終了処理
	}

	TSLib::Finish(); // 終了
	return 0;
}
