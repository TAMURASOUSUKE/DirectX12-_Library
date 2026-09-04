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

	// マウスカーソルの状態を設定
	bool isLocked{ true }; // Initialize直後にLockedにした状態と合わせる
	if (!System::SetCursorMode(System::CursorMode::Normal)) DEBUG_LOG_ERROR("マウスカーソルの状態設定に失敗しました\n");

	TexHandle heightMap{ Gfx::LoadTexture("Res/TestVolume.png") }; // ハイトマップ取得

	// Model
	ModelHandle player{ Gfx::LoadModel("Res/TestMultipleAnimModel.glb") }; // Playerモデルのロード
	ModelHandle toon{ Gfx::LoadModel("Res/player_01_01.glb") }; // Toon用モデルのロード
	Gfx::SetModelAlphaMode(toon, 0, ModelAlphaMode::Blend); // 半透明チェック
	//Gfx::SetModelAlphaMode(toon, 1, ModelAlphaMode::Blend); // 半透明チェック
	Gfx::SetBaseColor(toon, 0, { 1.0f, 1.0f, 1.0f, 0.35f });
	//Gfx::SetBaseColor(toon, 1, { 1.0f, 1.0f, 1.0f, 0.35f });
	Transform objectPosition{};
	Transform toonTransform{};
	objectPosition.SetPosition({ -1.0f, 0.0f, 0.0f });
	toonTransform.SetPosition({ 1.0f, 0.0f, 0.0f });
	AnimInstanceHandle alienModelAnim{ Gfx::CreateAnimInstance(player) }; // testModelからAnimationのInstanceを作る
	AnimInstanceHandle toonModelAnim{ Gfx::CreateAnimInstance(toon) }; // toonModelからAnimationのInstanceを作る

	float t{ 0.0f }; // 時間
	float tessFactor{ 4.0f }; // HSでの分割数
	float heightFactor{ 0.0f }; // Terrainの高さ

	// タイムスケール
	constexpr float ANIM_FRAME_DURATION{ 1.0f / 10.0f };
	float timeScale{ 1.0f };
	float time{ 0.0f };
	int spriteAnimTime{ 0 };

	// 3D基礎図形
	Transform cube{};
	cube.SetPosition({ -2.5f, 1.0f, 3.0f });
	Transform cylinder{};
	cylinder.SetPosition({ -1.0f, 1.0f, 3.0f });
	Transform plane{};
	plane.SetPosition({ 0.0f, -2.0f, 4.0f });
	Transform sphere{};
	sphere.SetPosition({ 1.5f, 1.0f, 3.0f });

	// あたり判定を行えるか
	Transform debugCubeAABB{};
	debugCubeAABB.SetPosition({ -2.0f, 0.0f, 3.0f });
	AABB debugCube{debugCubeAABB.GetPosition() + Vector3{-0.5f, 0.0f, -0.5f}, debugCubeAABB.GetPosition() + Vector3{0.5f, 2.0f,  0.5f} };
	Vector4 hitColor{ 0.0f, 0.0f, 0.0f, 1.0 };

	// カメラ設定
	Camera camera{};
	camera.transform.SetPosition({ 0.0f, 1.0f, -3.0f });
	// 回転角度
	float cameraYaw{ 0.0f };
	float cameraPtich{ 0.0f };
	// 1pxの移動で何ラジアン回すか
	constexpr float MOUSE_SENSITIVITY{ 0.2f * Math::DEG_TO_RAD };
	// 真下真上まで回すとLookAtの軸が不安定になるため少し手前で止める
	constexpr float  MAX_CAMERA_PITCH{ 89.0f * Math::DEG_TO_RAD };

	SceneLight sceneLight{};
	sceneLight.directional.direction = { 1.0f, -1.0f, 1.0f };
	sceneLight.directional.color = { 1.0f, 1.0f, 1.0f };
	sceneLight.directional.intensity = 4.0f;

	sceneLight.ambient.color = { 1.0f, 1.0f, 1.0f };
	sceneLight.ambient.intensity = 0.15f;

	bool isFullscreen{ false }; // 実行中のWindowSize変更チェック
	// ゲームループ
	while (TSLib::ProcessMessage() && !Input::IsKeyPushed(KeyCode::Button::ESC))
	{
		TSLib::BeginFrame(); // フレーム開始処理
		time += Time::DeltaTime();

		if (Input::IsKeyPushed(KeyCode::Button::TAB))
		{
			isLocked = !isLocked;

			const System::CursorMode mode{ isLocked ? System::CursorMode::Locked : System::CursorMode::Normal };

			if (!System::SetCursorMode(mode)) DEBUG_LOG_ERROR("マウスカーソルの状態変更に失敗しました\n");
		}

		// ライト回転をして影響を確認
		sceneLight.directional.direction = { std::cos(time), -0.6f, std::sin(time) };

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

		// カメラ回転
		if (Input::IsMousePress(MouseCode::Click::RIGHT)) // 右クリック中だけ動かす
		{
			const Vector2Int mouseDelta{ Input::GetMouseDelta() };
			cameraYaw += static_cast<float>(mouseDelta.x) * MOUSE_SENSITIVITY;
			cameraPtich += static_cast<float>(mouseDelta.y) * MOUSE_SENSITIVITY; // クライアント座標では下方向がプラス
			cameraYaw = Math::NormalizeAngle(cameraYaw); // Yawが際限なく大きくなるのを防ぐ
			// 真下、真上を超えてカメラが反転しないようにする
			cameraPtich = std::clamp(cameraPtich, -MAX_CAMERA_PITCH, MAX_CAMERA_PITCH);
			// 保存している角度から毎回Quaternionを作り出す
			camera.transform.SetRotation(Quaternion::FromEuler({ cameraPtich, cameraYaw, 0.0f }));
		}

		// カメラ移動
		Vector3 dir{ Vector3::Zero };
		const Quaternion cameraRotation{ camera.transform.GetRotation() };
		const Vector3 cameraForward{ cameraRotation.RotateVector(Vector3::Forward) };
		const Vector3 cameraRight{ cameraRotation.RotateVector(Vector3::Right) };
		if (Input::IsKeyPress(KeyCode::Button::W)) dir += cameraForward;
		if (Input::IsKeyPress(KeyCode::Button::A)) dir -= cameraRight;
		if (Input::IsKeyPress(KeyCode::Button::S)) dir -= cameraForward;
		if (Input::IsKeyPress(KeyCode::Button::D)) dir += cameraRight;
		float speed{ 8.0f };
		dir.Normalize();
		camera.transform.Translate(dir * speed * Time::DeltaTime());

		// AABB確認
		AABB playerAABB{ objectPosition.GetPosition() + Vector3{-0.5f, 0.0f, -0.5f}, objectPosition.GetPosition() + Vector3{0.5f, 2.0f,  0.5f} };
		if (Collision::Intersect(playerAABB, debugCube)) hitColor = { 1.0f, 0.0f, 0.0f, 1.0f };
		else hitColor = { 0.0f, 1.0f, 0.0f, 1.0f };


		// 3Dモデルアニメーション
		// 通常ループ再生
		if (Input::IsKeyPushed(KeyCode::Button::SPACE)) Gfx::PlayAnim(alienModelAnim, 0, true, 1.0f);
		// 逆方向ループ再生
		if (Input::IsKeyPushed(KeyCode::Button::R)) Gfx::PlayAnim(alienModelAnim, 0, true, -1.0f);
		// 現在の姿勢で一時停止
		if (Input::IsKeyPushed(KeyCode::Button::P)) Gfx::PauseAnim(alienModelAnim);
		// 一時停止した位置から再開
		if (Input::IsKeyPushed(KeyCode::Button::O)) Gfx::ResumeAnim(alienModelAnim);
		// 再生方向に応じた開始位置へ戻して停止
		if (Input::IsKeyPushed(KeyCode::Button::B)) Gfx::StopAnim(alienModelAnim);
		Gfx::UpdateAnim(alienModelAnim, Time::DeltaTime());

		std::string fpsValue{ std::format("CurrentMeasuredFPS : {:.1f}", Time::FPS()) };
		std::string targetFPS{ std::format("CurrentSettingFPS : {}", Time::GetTargetFPS()) };
		std::string unscaledDeltaTime{ std::format("CurrentUnscaledDeltaTime: {:.6f}", Time::UnscaledDeltaTime()) };
		std::string deltaTime{ std::format("CurrentDeltaTime : {:.3f}", Time::DeltaTime()) };
		std::string timeScale{ std::format("CurrentTimeScale : {:.2f}", Time::GetTimeScale()) };

		// ウィンドウモード変更チェック
		if (Input::IsKeyPushed(KeyCode::Button::RETURN)) isFullscreen = !isFullscreen;
		if (isFullscreen) System::SetWindowMode(System::WindowMode::BorderlessFullscreen);
		else  System::SetWindowMode(System::WindowMode::Windowed);

		Gfx::SetCamera(camera); // 3D描画前に呼ぶ
		Gfx::SetSceneLight(sceneLight);
		Gfx::ClearScreen(1.0f, 1.0f, 1.0f, 1.0f); // 画面クリア
		
		// 3Dモデルアニメーション
		Gfx::DrawAnimatedModel(alienModelAnim, objectPosition);
		// 静的モデル
		Gfx::DrawAnimatedModel(toonModelAnim, toonTransform);

		// 3D基礎図形
		Gfx::DrawCube3D(cube, { 1.0f, 0.0f, 1.0f, 1.0f }, Gfx::Primitive3DStyle::DebugLine);
		Gfx::DrawSphere3D(sphere, { 0.0f, 0.5f, 0.0f, 1.0f }, Gfx::Primitive3DStyle::MeshWireframe);
		Gfx::DrawCylinder3D(cylinder, { 0.0f, 1.0f, 0.0f, 1.0f }, Gfx::Primitive3DStyle::DebugLine);
		Gfx::DrawCapsule3D({ 0.0f, -2.0f, 5.0f }, { 0.0f,  0.0f, 5.0f }, 0.5f, { 0.2f, 1.0f, 0.3f }, Gfx::Primitive3DStyle::Fill);
		Gfx::DrawPlane3D(plane, { 0.0f, 0.3f, 0.4f }, Gfx::Primitive3DStyle::Fill);
		//Gfx::DrawLine3D({ 2.0f, 1.0f, 6.0f }, {-1.0f, -3.0f, 6.0f});
		// Gfx::DrawGrid3D({ 0.0f, -1.0f, 5.0f }, Quaternion::FromEuler(-45.0f * Math::DEG_TO_RAD, 0.0f, 0.0f), 10, 1.0f, {0.4f, 0.4f, 0.4f, 1.0f});
		/* Gfx::DrawWorldAxisGrid3D({ 0.0f, -1.0f, 5.0f }, 10, 1.0f,{1.0f, 1.0f, 1.0f, 1.0}, {1.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f});*/
		Gfx::DrawAxis3D(objectPosition.GetPosition(), objectPosition.GetRotation());
		 //Gfx::DrawAABB3D(playerAABB,hitColor);
		 //Gfx::DrawAABB3D(debugCube,{ 0.0f, 0.0f, 1.0f, 1.0f });
		// Gfx::DrawTerrain({ 0.0f, 0.0f, 3.0f }, 10.0f, tessFactor, heightFactor, {1.0f, 0.0f, 0.0f, 1.0f}, heightMap);


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
