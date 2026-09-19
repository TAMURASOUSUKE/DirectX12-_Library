#include "../Src/Facade/TSLib.h"
#include <algorithm>
#include <string>

// 判定用Boxと描画用Transformを同じ姿勢にする 現状のDrawBox3D(Box)はrotationを描画に反映しないため、Transform版を使う。
Transform MakeDrawTransform(const Box& _box)
{
	Transform transform{};
	transform.SetPosition(_box.center);
	transform.SetRotation(_box.rotation);
	transform.SetScale(_box.GetSize()); // halfSizeの2倍が描画する箱の全幅
	return transform;
}

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

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	if (!TSLib::Initialize(L"CollisionTest - OBB", 1280, 720)) return -1;
	Time::SetTargetFPS(60);
	System::SetCursorMode(System::CursorMode::Normal);

	const TestResult tests{ RunCollisionTests() };
	const Box fixedBox{ {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 0.7f}, Quaternion::FromAxisAngle(Vector3::Up, 25.0f * Math::DEG_TO_RAD) };
	Box movingBox{ {2.8f, 0.0f, 0.0f}, {1.0f, 0.8f, 0.7f} };
	float boxYaw{ 0.0f };

	// 三人称カメラ：操作する箱を注視し右ドラッグで周囲を回る
	Camera camera{};
	float cameraYaw{ 0.0f };
	float cameraPitch{ 20.0f * Math::DEG_TO_RAD };
	constexpr float cameraDistance{ 7.0f };
	constexpr float mouseSensitivity{ 0.2f * Math::DEG_TO_RAD };
	constexpr float minPitch{ 10.0f * Math::DEG_TO_RAD };
	constexpr float maxPitch{ 75.0f * Math::DEG_TO_RAD };

	while (TSLib::ProcessMessage() && !Input::IsKeyPushed(KeyCode::Button::ESC))
	{
		TSLib::BeginFrame();

		const float dt{ Time::DeltaTime() };
		if (Input::IsMousePress(MouseCode::Click::RIGHT))
		{
			const Vector2Int delta{ Input::GetMouseDelta() };
			cameraYaw = Math::NormalizeAngle(cameraYaw + static_cast<float>(delta.x) * mouseSensitivity);
			cameraPitch = std::clamp(cameraPitch + static_cast<float>(delta.y) * mouseSensitivity, minPitch, maxPitch);
		}

		// 移動はカメラの水平向きに合わせる 上下成分は含めない。
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
			movingBox.center += moveDirection * (3.0f * dt);
		}

		// 箱の回転はカメラから独立させ OBB判定そのものを観察する
		if (Input::IsKeyPress(KeyCode::Button::Q)) boxYaw -= 1.5f * dt;
		if (Input::IsKeyPress(KeyCode::Button::E)) boxYaw += 1.5f * dt;
		movingBox.rotation = Quaternion::FromAxisAngle(Vector3::Up, boxYaw);

		const Quaternion cameraRotation{ Quaternion::FromEuler(cameraPitch, cameraYaw, 0.0f) };
		const Vector3 target{ movingBox.center + Vector3{0.0f, 0.7f, 0.0f} };
		const Vector3 cameraForward{ cameraRotation.RotateVector(Vector3::Forward) };
		camera.transform.SetRotation(cameraRotation);
		camera.transform.SetPosition(target - cameraForward * cameraDistance);

		const bool hit{ Collision::Intersect(fixedBox, movingBox) };
		Gfx::SetCamera(camera);
		Gfx::ClearScreen();
		// Gfx::DrawWorldAxisGrid3D({0.0f, -1.0f, 0.0f}, 10, 1.0f);
		Gfx::DrawBox3D(MakeDrawTransform(movingBox), hit ? Vector4{ 1.0f, 0.2f, 0.2f, 1.0f } : Vector4{ 0.2f, 1.0f, 0.3f, 1.0f }, Gfx::Primitive3DStyle::DebugLine);
		Gfx::DrawBox3D(MakeDrawTransform(fixedBox), {0.2f, 0.6f, 1.0f, 1.0f}, Gfx::Primitive3DStyle::DebugLine);

		Gfx::DrawString("WASD: move  Q/E: rotate box  Right drag: camera  ESC: exit", {20.0f, 20.0f});
		Gfx::DrawString(hit ? "COLLISION: HIT" : "COLLISION: NONE", {20.0f, 55.0f}, 1.0f, hit ? Vector4{1.0f, 0.3f, 0.3f, 1.0f} : Vector4{0.3f, 1.0f, 0.3f, 1.0f});
		const std::string testText{ tests.failedCount == 0 ? "STARTUP TESTS: PASS" : "STARTUP TESTS: FAIL - " + tests.firstFailure };
		Gfx::DrawString(testText.c_str(), {20.0f, 90.0f});
		TSLib::EndFrame();
	}

	TSLib::Finish();
	return tests.failedCount == 0 ? 0 : 1;
}
