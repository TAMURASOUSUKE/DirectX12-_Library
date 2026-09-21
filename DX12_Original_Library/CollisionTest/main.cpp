#include "../Src/Facade/TSLib.h"
#include <algorithm>
#include <string>

struct TestResult
{
	int failedCount{ 0 };
	std::string firstFailure{};
};

TestResult RunCollisionTests()
{
	TestResult result{};
	const auto check = [&result](const char* _name, const Box& _a, const Box& _b, bool _expected)
		{
			if (Collision::Intersect(_a, _b) != _expected)
			{
				result.failedCount++;
				if (result.firstFailure.empty()) result.firstFailure = _name;
			}
		};

	const Box base{ {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f} };
	const Quaternion yaw45{ Quaternion::FromAxisAngle(Vector3::Up, 45.0f * Math::DEG_TO_RAD) };
	check("separated AABB", base, Box{{3.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}}, false);
	check("touching faces", base, Box{{2.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}}, true);
	check("overlapping AABB", base, Box{{1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}}, true);
	check("overlapping rotated box", base, Box{{2.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, yaw45}, true);
	check("separated rotated box", base, Box{{3.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, yaw45}, false);
	check("separated on negative side", base, Box{{-3.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, yaw45}, false);
	return result;
}

TestResult RunCapsuleTests()
{
	TestResult result{};

	const auto checkCapsules = [&result](
		const char* name, const Capsule& a, const Capsule& b, bool expected)
		{
			// 衝突判定は A,B の順番で結果が変わってはいけない。
			const bool forward{ Collision::Intersect(a, b) };
			const bool reversed{ Collision::Intersect(b, a) };

			if (forward != expected || reversed != expected)
			{
				result.failedCount++;
				if (result.firstFailure.empty()) result.firstFailure = name;
			}
		};

	const auto checkSphere = [&result](
		const char* name, const Capsule& capsule, const Sphere& sphere, bool expected)
		{
			if (Collision::Intersect(capsule, sphere) != expected)
			{
				result.failedCount++;
				if (result.firstFailure.empty()) result.firstFailure = name;
			}
		};

	// 2本の線分が十字に交差する配置
	const Capsule horizontal{ {-1.0f, 0.0f, 0.0f}, { 1.0f, 0.0f, 0.0f}, 0.4f };
	checkCapsules("midpoint hit", horizontal, Capsule{ {0.0f, -1.0f, 0.5f}, {0.0f, 1.0f, 0.5f}, 0.4f }, true);  // 軸間距離0.5 < 半径の合計0.8
	checkCapsules("midpoint gap", horizontal, Capsule{ {0.0f, -1.0f, 1.0f}, {0.0f, 1.0f, 1.0f}, 0.4f }, false); // 軸間距離1.0 > 半径の合計0.8

	// 平行な線分 線分同士の計算で分母が0になるケースを確認する
	const Capsule vertical{ {0.0f, 0.0f, 0.0f}, {0.0f, 2.0f, 0.0f}, 0.5f };
	checkCapsules("parallel touching", vertical, Capsule{ {1.0f, 0.0f, 0.0f}, {1.0f, 2.0f, 0.0f}, 0.5f }, true);
	checkCapsules("parallel gap", vertical, Capsule{ {1.2f, 0.0f, 0.0f}, {1.2f, 2.0f, 0.0f}, 0.5f }, false);

	// 最近点が線分の端点になるケース
	const Capsule shortA{ {0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, 0.25f };
	checkCapsules("end touching", shortA, Capsule{ {1.5f, 0.0f, 0.0f}, {2.5f, 0.0f, 0.0f}, 0.25f }, true);  // 端点間距離0.5 == 半径の合計0.5
	checkCapsules("end gap", shortA, Capsule{ {1.6f, 0.0f, 0.0f}, {2.6f, 0.0f, 0.0f}, 0.25f }, false);

	// startPos == endPos のカプセルは、実質的に球になる
	const Capsule point{ {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.5f };
	checkCapsules("zero length hit", point, Capsule{ {0.9f, 0.0f, 0.0f}, {0.9f, 0.0f, 0.0f}, 0.5f }, true);
	checkCapsules("zero length gap", point, Capsule{ {1.1f, 0.0f, 0.0f}, {1.1f, 0.0f, 0.0f}, 0.5f }, false);

	// Capsule×Sphereも公開API経由で確認
	checkSphere("sphere side touching", vertical, Sphere{ {1.0f, 1.0f, 0.0f}, 0.5f }, true);
	checkSphere("sphere side gap", vertical, Sphere{ {1.2f, 1.0f, 0.0f}, 0.5f }, false);
	checkSphere("sphere end touching", vertical, Sphere{ {0.0f, 3.0f, 0.0f}, 0.5f }, true);

	return result;
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	if (!TSLib::Initialize(L"CollisionTest", 1280, 720)) return -1;
	Time::SetTargetFPS(60);
	System::SetCursorMode(System::CursorMode::Normal);

	// BoxテストとCapsuleテストを両方実行する
	TestResult tests{ RunCollisionTests() };
	const TestResult capsuleTests{ RunCapsuleTests() };
	tests.failedCount += capsuleTests.failedCount;
	if (tests.firstFailure.empty()) tests.firstFailure = capsuleTests.firstFailure;

	// 青い固定カプセル 軸はY方向 長さ2 半径0.55
	const Capsule fixedCapsule{ {0.0f, 0.0f, 0.0f}, {0.0f, 2.0f, 0.0f}, 0.55f };

	// 操作カプセルは「中心」と「角度」を保持し毎フレームそこから両端点を組み立てる。
	Vector3 movingCenter{ 2.2f, 1.0f, 0.0f };
	float capsuleAngle{ 0.0f };

	// 三人称カメラ
	Camera camera{};
	float cameraYaw{ 0.0f };
	float cameraPitch{ 20.0f * Math::DEG_TO_RAD };
	constexpr float CAMERA_DISTANCE{ 7.0f };
	constexpr float MOUSE_SENSITIVITY{ 0.2f * Math::DEG_TO_RAD };
	constexpr float MIN_PITCH{ 10.0f * Math::DEG_TO_RAD };
	constexpr float MAX_PITCH{ 75.0f * Math::DEG_TO_RAD };

	while (TSLib::ProcessMessage() && !Input::IsKeyPushed(KeyCode::Button::ESC))
	{
		TSLib::BeginFrame();
		const float dt{ Time::DeltaTime() };

		// マウス移動量をカメラの水平・垂直角に反映する。
		if (Input::IsMousePress(MouseCode::Click::RIGHT))
		{
			const Vector2Int delta{ Input::GetMouseDelta() };
			cameraYaw = Math::NormalizeAngle(cameraYaw + static_cast<float>(delta.x) * MOUSE_SENSITIVITY);
			cameraPitch = std::clamp(cameraPitch + static_cast<float>(delta.y) * MOUSE_SENSITIVITY, MIN_PITCH, MAX_PITCH);
		}

		// WASDの移動方向をカメラの水平向きに合わせる
		const Quaternion horizontalRotation{ Quaternion::FromAxisAngle(Vector3::Up, cameraYaw) };
		const Vector3 forward{ horizontalRotation.RotateVector(Vector3::Forward) };
		const Vector3 right{ horizontalRotation.RotateVector(Vector3::Right) };

		Vector3 moveDirection{ Vector3::Zero };
		if (Input::IsKeyPress(KeyCode::Button::W)) moveDirection += forward;
		if (Input::IsKeyPress(KeyCode::Button::S)) moveDirection -= forward;
		if (Input::IsKeyPress(KeyCode::Button::D)) moveDirection += right;
		if (Input::IsKeyPress(KeyCode::Button::A)) moveDirection -= right;

		if (moveDirection.LengthSquared() > 0.0f)
		{
			moveDirection.Normalize();
			movingCenter += moveDirection * (3.0f * dt);
		}

		// Q/Eでカプセルの軸方向を傾ける。
		if (Input::IsKeyPress(KeyCode::Button::Q)) capsuleAngle -= 1.5f * dt;
		if (Input::IsKeyPress(KeyCode::Button::E)) capsuleAngle += 1.5f * dt;

		const Vector3 axis{ Quaternion::FromAxisAngle(Vector3::Forward, capsuleAngle).RotateVector(Vector3::Up) };

		// axisの長さは1 中心から上下に1ずつ伸ばし軸線分の長さ2のカプセルを作る。
		const Capsule movingCapsule{ movingCenter - axis, movingCenter + axis, 0.55f };

		// 操作カプセルの少し上をカメラの注視点にする。
		const Quaternion cameraRotation{ Quaternion::FromEuler(cameraPitch, cameraYaw, 0.0f) };
		const Vector3 target{ movingCenter + Vector3{0.0f, 0.4f, 0.0f} };
		const Vector3 cameraForward{ cameraRotation.RotateVector(Vector3::Forward) };
		camera.transform.SetRotation(cameraRotation);
		camera.transform.SetPosition(target - cameraForward * CAMERA_DISTANCE);

		const bool hit{ Collision::Intersect(fixedCapsule, movingCapsule) };

		Gfx::SetCamera(camera);
		Gfx::ClearScreen();

		// 形状をDebugLineで表示
		Gfx::DrawCapsule3D(fixedCapsule.startPos, fixedCapsule.endPos, fixedCapsule.radius, { 0.2f, 0.6f, 1.0f, 1.0f }, Gfx::Primitive3DStyle::Fill);

		// 操作対象は通常緑、衝突中だけ赤にする
		Gfx::DrawCapsule3D(movingCapsule.startPos, movingCapsule.endPos, movingCapsule.radius, hit ? Vector4{ 1.0f, 0.2f, 0.2f, 1.0f } : Vector4{ 0.2f, 1.0f, 0.3f, 1.0f }, Gfx::Primitive3DStyle::Fill);

		Gfx::DrawString("WASD: move  Q/E: rotate capsule  Right drag: camera  ESC: exit", { 20.0f, 20.0f });
		Gfx::DrawString(hit ? "COLLISION: HIT" : "COLLISION: NONE", { 20.0f, 55.0f }, 1.0f, hit ? Vector4{ 1.0f, 0.3f, 0.3f, 1.0f } : Vector4{ 0.3f, 1.0f, 0.3f, 1.0f });

		// 数値テストの結果も画面に表示する
		const std::string testText{ tests.failedCount == 0 ? "STARTUP TESTS: PASS" : "STARTUP TESTS: FAIL - " + tests.firstFailure };
		Gfx::DrawString(testText.c_str(), { 20.0f, 90.0f });

		TSLib::EndFrame();
	}

	TSLib::Finish();

	// 自動テストが失敗した場合は終了コードも失敗にする。
	return tests.failedCount == 0 ? 0 : 1;
}
