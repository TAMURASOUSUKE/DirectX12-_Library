#include "../Src/Facade/TSLib.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <span>
#include <string>
#include <variant>
#include <vector>

// 起動時の数値テストの結果
struct TestResult
{
	int failedCount{ 0 };
	std::string firstFailure{};
};

// 画面に出すためにテスト側で持つBody一つ分の情報(Physicsは描かないので形状と色はここで覚える)
struct TestBody
{
	const char* name{ "" }; // 画面に出す名前
	BodyDesc desc{}; // 作成に使った設定(Rキーで作り直すときにも使う)
	Vector4 color{ 1.0f, 1.0f, 1.0f, 1.0f }; // 描画色
	BodyHandle handle{}; // 作成したBody
};

// 起動時に一回だけ走る数値テスト 確かめる項目と期待値はここへ書く
TestResult RunPhysicsTests()
{
	TestResult result{};
	// 条件が成り立たなかったら失敗として数える
	const auto check = [&result](const char* _name, bool _passed)
		{
			if (_passed) return;
			result.failedCount++;
			if (result.firstFailure.empty()) result.firstFailure = _name;
		};
	// 書き方 : check("項目の名前", 成り立つべき条件);

	// Staticをわざと動かそうとしても動かないこと
	{
		constexpr int STEP_COUNT{ 60 }; // 1秒分進める
		BodyDesc desc{};
		desc.shapeData = Box{ Vector3::Zero, { 1.0f, 1.0f, 1.0f } };
		desc.position = { 1.0f, 2.0f, 3.0f };
		desc.motionType = MotionType::Static;
		desc.velocity = { 5.0f, 5.0f, 5.0f }; // わざと初速を入れる
		desc.isGravityEnabled = true; // わざと重力も有効にする

		const BodyHandle body{ Physics::CreateBody(desc) };
		check("static: created", body.IsValid());
		// 作成時に渡した初速は捨てられる
		check("static: initial velocity ignored", Physics::GetVelocity(body) == Vector3::Zero);
		// 後から速度を入れようとしても断られ、値も変わらない
		check("static: SetVelocity rejected", !Physics::SetVelocity(body, { 0.0f, 10.0f, 0.0f }));
		check("static: velocity stays zero", Physics::GetVelocity(body) == Vector3::Zero);
		// 重力が有効でもStepで位置と速度が変わらない
		for (int i{ 0 }; i < STEP_COUNT; i++) Physics::Step(Time::FixedDeltaTime());
		check("static: position unchanged after steps", Physics::GetPosition(body) == desc.position);
		check("static: velocity unchanged after steps", Physics::GetVelocity(body) == Vector3::Zero);
		check("static: destroyed", Physics::DestroyBody(body));
	}

	return result;
}

// 画面に置くBodyの設定を作る(置く物を変えるときはここを書き換える)
std::vector<TestBody> MakeTestBodies()
{
	std::vector<TestBody> bodies{};

	// 動かない床
	BodyDesc floorDesc{};
	floorDesc.shapeData = Box{ Vector3::Zero, { 5.0f, 0.5f, 5.0f } };
	floorDesc.position = { 0.0f, -0.5f, 0.0f };
	floorDesc.motionType = MotionType::Static;
	bodies.push_back({ "Static Box", floorDesc, { 0.6f, 0.6f, 0.6f, 1.0f } });

	// 重力で落ちる球
	BodyDesc sphereDesc{};
	sphereDesc.shapeData = Sphere{ Vector3::Zero, 0.5f };
	sphereDesc.position = { -2.0f, 5.0f, 0.0f };
	sphereDesc.motionType = MotionType::Dynamic;
	bodies.push_back({ "Dynamic Sphere", sphereDesc, { 0.2f, 1.0f, 0.3f, 1.0f } });

	// 重力を切って初速だけで動く箱
	BodyDesc boxDesc{};
	boxDesc.shapeData = Box{ Vector3::Zero, { 0.5f, 0.5f, 0.5f } };
	boxDesc.position = { -4.0f, 3.0f, 0.0f };
	boxDesc.motionType = MotionType::Dynamic;
	boxDesc.velocity = { 1.0f, 0.0f, 0.0f };
	boxDesc.isGravityEnabled = false;
	bodies.push_back({ "Dynamic Box (no gravity)", boxDesc, { 1.0f, 0.8f, 0.2f, 1.0f } });

	// 速度だけで動くカプセル(重力は受けない)
	BodyDesc capsuleDesc{};
	capsuleDesc.shapeData = Capsule{ { 0.0f, -0.5f, 0.0f }, { 0.0f, 0.5f, 0.0f }, 0.3f };
	capsuleDesc.position = { 2.0f, 1.0f, 0.0f };
	capsuleDesc.motionType = MotionType::Kinematic;
	capsuleDesc.velocity = { 0.0f, 0.5f, 0.0f };
	bodies.push_back({ "Kinematic Capsule", capsuleDesc, { 0.2f, 0.6f, 1.0f, 1.0f } });

	// 床に少しめり込ませて置く球(重力なし 接触の点と法線を止まった状態で見るため)
	BodyDesc restingDesc{};
	restingDesc.shapeData = Sphere{ Vector3::Zero, 0.5f };
	restingDesc.position = { 0.0f, 0.4f, 0.0f };
	restingDesc.motionType = MotionType::Dynamic;
	restingDesc.isGravityEnabled = false;
	bodies.push_back({ "Resting Sphere (no gravity)", restingDesc, { 1.0f, 0.4f, 0.8f, 1.0f } });

	return bodies;
}

// 設定からBodyを作成する
void CreateBodies(std::vector<TestBody>& _bodies)
{
	for (TestBody& body : _bodies)
	{
		body.handle = Physics::CreateBody(body.desc);
	}
}

// 作成したBodyを全て破棄する
void DestroyBodies(std::vector<TestBody>& _bodies)
{
	for (TestBody& body : _bodies)
	{
		if (body.handle.IsValid()) Physics::DestroyBody(body.handle);
		body.handle = BodyHandle{};
	}
}

// 形状ごとの描画(形状の中の位置はBodyの位置からの相対なのでワールドへ直してから描く)
void DrawShape(const Box& _box, const BodyState& _state, Vector4 _color)
{
	const Vector3 center{ _state.position + _state.rotation.RotateVector(_box.center) };
	Gfx::DrawBox3D(Box{ center, _box.halfSize, _state.rotation * _box.rotation }, _color);
}

void DrawShape(const Sphere& _sphere, const BodyState& _state, Vector4 _color)
{
	// 始点と終点が同じCapsuleは球として描かれる
	const Vector3 center{ _state.position + _state.rotation.RotateVector(_sphere.center) };
	Gfx::DrawCapsule3D(center, center, _sphere.radius, _color, Primitive3DStyle::DebugLine);
}

void DrawShape(const Capsule& _capsule, const BodyState& _state, Vector4 _color)
{
	const Vector3 start{ _state.position + _state.rotation.RotateVector(_capsule.startPos) };
	const Vector3 end{ _state.position + _state.rotation.RotateVector(_capsule.endPos) };
	Gfx::DrawCapsule3D(start, end, _capsule.radius, _color, Primitive3DStyle::DebugLine);
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	if (!TSLib::Initialize(L"PhysicsTest", 1280, 720)) return -1;
	Time::SetTargetFPS(60);
	System::SetCursorMode(System::CursorMode::Normal);

	// 起動時の数値テスト
	const TestResult tests{ RunPhysicsTests() };

	// Step一回の時間をDebug Overlayへ出す
	const DebugChannelID physicsChannelID{ Debug::RegisterChannel("Physics") };
	const DebugMetricID stepCpuMetricID{ Debug::RegisterMetric({ "Physics Step CPU", physicsChannelID, DebugMetricUnit::Milliseconds, DebugMetricAggregation::Set, DebugMetricDisplayMode::WindowStatistics }) };

	std::vector<TestBody> bodies{ MakeTestBodies() };
	CreateBodies(bodies);

	bool isPaused{ false }; // Stepを止めているか
	int stepCount{ 0 }; // 作り直してからStepを呼んだ回数
	double lastStepMilliseconds{ 0.0 }; // 直近のStep一回の時間
	double maxStepMilliseconds{ 0.0 }; // 作り直してからの最大
	int lastContactCount{ -1 }; // 前のフレームの接触の数(変わったときだけログへ出す)
	constexpr float CONTACT_POINT_RADIUS{ 0.06f }; // 接触の点を表す球の半径
	constexpr float CONTACT_NORMAL_LENGTH{ 0.6f }; // 法線を表す線の長さ

	// 注視点の周りを回るカメラ
	Camera camera{};
	float cameraYaw{ 0.0f };
	float cameraPitch{ 15.0f * Math::DEG_TO_RAD };
	const Vector3 cameraTarget{ 0.0f, 2.0f, 0.0f };
	constexpr float CAMERA_DISTANCE{ 14.0f };
	constexpr float MOUSE_SENSITIVITY{ 0.2f * Math::DEG_TO_RAD };
	constexpr float MIN_PITCH{ -30.0f * Math::DEG_TO_RAD };
	constexpr float MAX_PITCH{ 75.0f * Math::DEG_TO_RAD };

	while (TSLib::ProcessMessage() && !Input::IsKeyPushed(KeyCode::Button::ESC))
	{
		TSLib::BeginFrame();

		// 右ドラッグでカメラを回す
		if (Input::IsMousePress(MouseCode::Click::RIGHT))
		{
			const Vector2Int delta{ Input::GetMouseDelta() };
			cameraYaw = Math::NormalizeAngle(cameraYaw + static_cast<float>(delta.x) * MOUSE_SENSITIVITY);
			cameraPitch = std::clamp(cameraPitch + static_cast<float>(delta.y) * MOUSE_SENSITIVITY, MIN_PITCH, MAX_PITCH);
		}

		// Space : 一時停止の切り替え
		if (Input::IsKeyPushed(KeyCode::Button::SPACE)) isPaused = !isPaused;
		// N : 一時停止中に一回だけ進める
		bool isStepOnceRequested{ isPaused && Input::IsKeyPushed(KeyCode::Button::N) };
		// F1 : Debug Overlayの切り替え
		if (Input::IsKeyPushed(KeyCode::Button::F1)) Debug::SetOverlayVisible(!Debug::IsOverlayVisible());
		// R : 全て破棄して作り直す(破棄と番号の再利用も通る)
		if (Input::IsKeyPushed(KeyCode::Button::R))
		{
			DestroyBodies(bodies);
			CreateBodies(bodies);
			stepCount = 0;
			lastStepMilliseconds = 0.0;
			maxStepMilliseconds = 0.0;
		}

		// 固定更新(ゲーム側がStepを呼ぶ形をそのまま再現する)
		while (Time::IsFixedUpdateRequired())
		{
			if (!isPaused || isStepOnceRequested)
			{
				const auto timing{ Debug::BeginCPUTiming(stepCpuMetricID) };
				const auto startTime{ std::chrono::steady_clock::now() };
				Physics::Step(Time::FixedDeltaTime());
				const std::chrono::duration<double, std::milli> elapsed{ std::chrono::steady_clock::now() - startTime };

				lastStepMilliseconds = elapsed.count();
				maxStepMilliseconds = (std::max)(maxStepMilliseconds, lastStepMilliseconds);
				stepCount++;
				isStepOnceRequested = false;
			}
			Time::ConsumeFixedTime();
		}

		// カメラ
		const Quaternion cameraRotation{ Quaternion::FromEuler(cameraPitch, cameraYaw, 0.0f) };
		const Vector3 cameraForward{ cameraRotation.RotateVector(Vector3::Forward) };
		camera.transform.SetRotation(cameraRotation);
		camera.transform.SetPosition(cameraTarget - cameraForward * CAMERA_DISTANCE);

		Gfx::SetCamera(camera);
		Gfx::ClearScreen();

		// 形状の描画
		for (const TestBody& body : bodies)
		{
			const BodyState state{ Physics::GetState(body.handle) };
			const Vector4 color{ body.color };
			std::visit([&state, color](const auto& _shape) { DrawShape(_shape, state, color); }, body.desc.shapeData);
		}

		// 接触の点(赤い球)と法線(黄色い線 A->B)の描画 Stepの後に一覧を読む
		const std::span<const BodyToBodyResult> contacts{ Physics::GetContacts() };
		for (const BodyToBodyResult& contact : contacts)
		{
			const ShapeToShapeResult& shapeResult{ contact.shapeToShapeResult };
			for (int i = 0; i < shapeResult.pointCount; i++)
			{
				const Vector3 point{ shapeResult.points[i].worldPosition };
				Gfx::DrawCapsule3D(point, point, CONTACT_POINT_RADIUS, { 1.0f, 0.2f, 0.2f, 1.0f }, Primitive3DStyle::Fill);
				Gfx::DrawLine3D(point, point + shapeResult.normal * CONTACT_NORMAL_LENGTH, { 1.0f, 1.0f, 0.2f, 1.0f });
			}
		}

		// 接触の数が変わったときだけログへ出す(毎フレームは出さない)
		const int contactCount{ static_cast<int>(contacts.size()) };
		if (contactCount != lastContactCount)
		{
			DEBUG_LOG("接触の数 : {}\n", contactCount);
			for (const BodyToBodyResult& contact : contacts)
			{
				const ShapeToShapeResult& shapeResult{ contact.shapeToShapeResult };
				if (shapeResult.pointCount <= 0) continue;
				const CollisionPoint& point{ shapeResult.points[0] };
				DEBUG_LOG("  法線({}, {}, {})  深さ {}  点({}, {}, {})\n",
					shapeResult.normal.x, shapeResult.normal.y, shapeResult.normal.z, point.depth,
					point.worldPosition.x, point.worldPosition.y, point.worldPosition.z);
			}
			lastContactCount = contactCount;
		}

		// 操作と計測の表示
		char text[256]{};
		float textY{ 20.0f };
		constexpr float LINE_HEIGHT{ 30.0f };
		Gfx::DrawString("Space: pause  N: step once  R: recreate  F1: overlay  Right drag: camera  ESC: exit", { 20.0f, textY });
		textY += LINE_HEIGHT;

		const double simulatedSeconds{ static_cast<double>(stepCount) * static_cast<double>(Time::FixedDeltaTime()) };
		std::snprintf(text, sizeof(text), "%s  step: %d  time: %.3f s", isPaused ? "PAUSED" : "RUNNING", stepCount, simulatedSeconds);
		Gfx::DrawString(text, { 20.0f, textY });
		textY += LINE_HEIGHT;

		std::snprintf(text, sizeof(text), "Physics::Step  last: %.4f ms  max: %.4f ms", lastStepMilliseconds, maxStepMilliseconds);
		Gfx::DrawString(text, { 20.0f, textY });
		textY += LINE_HEIGHT;

		std::snprintf(text, sizeof(text), "contacts: %d", contactCount);
		Gfx::DrawString(text, { 20.0f, textY });
		textY += LINE_HEIGHT;

		// Bodyごとの位置と速度
		for (const TestBody& body : bodies)
		{
			const BodyState state{ Physics::GetState(body.handle) };
			std::snprintf(text, sizeof(text), "%s  pos(%.3f, %.3f, %.3f)  vel(%.3f, %.3f, %.3f)", body.name,
				state.position.x, state.position.y, state.position.z, state.velocity.x, state.velocity.y, state.velocity.z);
			Gfx::DrawString(text, { 20.0f, textY }, 1.0f, body.color);
			textY += LINE_HEIGHT;
		}

		// 起動時の数値テストの結果
		const std::string testText{ tests.failedCount == 0 ? "STARTUP TESTS: PASS" : "STARTUP TESTS: FAIL - " + tests.firstFailure };
		Gfx::DrawString(testText.c_str(), { 20.0f, textY });

		TSLib::EndFrame();
	}

	DestroyBodies(bodies);
	TSLib::Finish();

	// 数値テストが失敗した場合は終了コードも失敗にする
	return tests.failedCount == 0 ? 0 : 1;
}
