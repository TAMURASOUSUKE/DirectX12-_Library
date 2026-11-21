#include "../Src/Facade/TSLib.h"
#include <algorithm>
#include <chrono>
#include <cstdint>
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

// 画面に置く三角形の集まりの頂点(山の形 稜線はz軸に沿う 描画にも使うのでテスト側で覚える)
const std::vector<Vector3> TENT_VERTICES{
	{ -2.0f, 0.0f, -2.0f }, { -2.0f, 0.0f, 2.0f }, // 左の裾
	{ 0.0f, 1.5f, -2.0f }, { 0.0f, 1.5f, 2.0f }, // 稜線
	{ 2.0f, 0.0f, -2.0f }, { 2.0f, 0.0f, 2.0f }, // 右の裾
};
// 画面に置く三角形の集まりの番号(3つで三角形1枚 左の斜面2枚 右の斜面2枚)
const std::vector<std::uint32_t> TENT_INDICES{ 0, 1, 2,  1, 3, 2,  2, 3, 4,  3, 5, 4 };
// 山を置く位置
const Vector3 TENT_POSITION{ 8.5f, 0.0f, 0.0f };

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

	// 球以外の形が床で止まること と 摩擦が効くこと(床の上面は y=0)
	{
		constexpr int STEP_COUNT{ 180 }; // 3秒分進める
		constexpr float POSITION_TOLERANCE{ 0.03f }; // 止まる高さの許す誤差
		constexpr float SPEED_TOLERANCE{ 0.05f }; // 止まったとみなす速さ

		BodyDesc floorDesc{};
		floorDesc.shapeData = Box{ Vector3::Zero, { 10.0f, 0.5f, 10.0f } };
		floorDesc.position = { 0.0f, -0.5f, 0.0f };
		floorDesc.motionType = MotionType::Static;
		floorDesc.friction = 0.6f;
		const BodyHandle floorBody{ Physics::CreateBody(floorDesc) };

		// 落ちるカプセル(軸の半分0.5 半径0.3 なので止まる高さは0.8)
		BodyDesc capsuleDesc{};
		capsuleDesc.shapeData = Capsule{ { 0.0f, -0.5f, 0.0f }, { 0.0f, 0.5f, 0.0f }, 0.3f };
		capsuleDesc.position = { 0.0f, 2.0f, -6.0f };
		capsuleDesc.motionType = MotionType::Dynamic;
		const BodyHandle capsule{ Physics::CreateBody(capsuleDesc) };

		// 落ちる箱(半分の大きさ0.5 なので止まる高さは0.5)
		BodyDesc boxDesc{};
		boxDesc.shapeData = Box{ Vector3::Zero, { 0.5f, 0.5f, 0.5f } };
		boxDesc.position = { 0.0f, 2.0f, -2.0f };
		boxDesc.motionType = MotionType::Dynamic;
		const BodyHandle box{ Physics::CreateBody(boxDesc) };

		// 摩擦ありで滑る箱(止まるはず)
		BodyDesc roughDesc{ boxDesc };
		roughDesc.position = { -5.0f, 0.5f, 2.0f };
		roughDesc.velocity = { 3.0f, 0.0f, 0.0f };
		roughDesc.friction = 0.6f;
		const BodyHandle roughBox{ Physics::CreateBody(roughDesc) };

		// 摩擦なしで滑る箱(速さが変わらないはず)
		BodyDesc smoothDesc{ boxDesc };
		smoothDesc.position = { -5.0f, 0.5f, 6.0f };
		smoothDesc.velocity = { 1.0f, 0.0f, 0.0f };
		smoothDesc.friction = 0.0f;
		const BodyHandle smoothBox{ Physics::CreateBody(smoothDesc) };

		for (int i{ 0 }; i < STEP_COUNT; i++) Physics::Step(Time::FixedDeltaTime());

		const BodyState capsuleState{ Physics::GetState(capsule) };
		check("capsule: rests on floor", std::abs(capsuleState.position.y - 0.8f) < POSITION_TOLERANCE);
		check("capsule: stopped", std::abs(capsuleState.velocity.y) < SPEED_TOLERANCE);

		const BodyState boxState{ Physics::GetState(box) };
		check("box: rests on floor", std::abs(boxState.position.y - 0.5f) < POSITION_TOLERANCE);
		check("box: stopped", std::abs(boxState.velocity.y) < SPEED_TOLERANCE);

		const BodyState roughState{ Physics::GetState(roughBox) };
		check("friction: rough box stopped", std::abs(roughState.velocity.x) < SPEED_TOLERANCE);
		check("friction: rough box stays on floor", std::abs(roughState.position.y - 0.5f) < POSITION_TOLERANCE);

		const BodyState smoothState{ Physics::GetState(smoothBox) };
		check("friction: smooth box keeps speed", std::abs(smoothState.velocity.x - 1.0f) < SPEED_TOLERANCE);

		Physics::DestroyBody(floorBody);
		Physics::DestroyBody(capsule);
		Physics::DestroyBody(box);
		Physics::DestroyBody(roughBox);
		Physics::DestroyBody(smoothBox);
	}

	// 三角形の集まり 登録できること と 球が平らな三角形の上で止まること(面は y=0)
	// この項目と期待値はAIが置いた(形の大きさから機械的に決まる確認だけ)
	{
		constexpr int STEP_COUNT{ 180 }; // 3秒分進める
		constexpr float POSITION_TOLERANCE{ 0.03f }; // 止まる高さの許す誤差
		constexpr float SPEED_TOLERANCE{ 0.05f }; // 止まったとみなす速さ

		// 一辺20の平らな四角を三角形2枚で作る(対角線は x=z)
		const std::vector<Vector3> vertices{ { -10.0f, 0.0f, -10.0f }, { -10.0f, 0.0f, 10.0f }, { 10.0f, 0.0f, 10.0f }, { 10.0f, 0.0f, -10.0f } };
		const std::vector<std::uint32_t> indices{ 0, 1, 2,  0, 2, 3 };
		const TriangleMeshHandle mesh{ Physics::CreateTriangleMesh(vertices, indices) };
		check("mesh: registered", mesh.IsValid());

		BodyDesc meshDesc{};
		meshDesc.shapeData = TriangleMeshShape{ mesh };
		meshDesc.motionType = MotionType::Static;
		const BodyHandle meshBody{ Physics::CreateBody(meshDesc) };
		check("mesh: static body created", meshBody.IsValid());

		// DynamicのBodyには使えない(エラーログが1行出るのは正しい動き)
		BodyDesc dynamicMeshDesc{ meshDesc };
		dynamicMeshDesc.motionType = MotionType::Dynamic;
		check("mesh: dynamic body rejected", !Physics::CreateBody(dynamicMeshDesc).IsValid());

		// 面積が0の三角形(3点が一直線)が入っていたら登録できない(エラーログが1行出るのは正しい動き)
		const std::vector<Vector3> flatVertices{ { 0.0f, 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }, { 2.0f, 0.0f, 0.0f } };
		const std::vector<std::uint32_t> flatIndices{ 0, 1, 2 };
		check("mesh: zero area rejected", !Physics::CreateTriangleMesh(flatVertices, flatIndices).IsValid());

		// 落ちる球(半径0.5 なので止まる高さは0.5 対角線から離れた位置へ落とす)
		BodyDesc sphereDesc{};
		sphereDesc.shapeData = Sphere{ Vector3::Zero, 0.5f };
		sphereDesc.position = { 3.0f, 2.0f, -3.0f };
		sphereDesc.motionType = MotionType::Dynamic;
		const BodyHandle sphere{ Physics::CreateBody(sphereDesc) };

		// 落ちるカプセル(軸の半分0.5 半径0.3 なので止まる高さは0.8)
		BodyDesc capsuleDesc{};
		capsuleDesc.shapeData = Capsule{ { 0.0f, -0.5f, 0.0f }, { 0.0f, 0.5f, 0.0f }, 0.3f };
		capsuleDesc.position = { 6.0f, 2.0f, -2.0f };
		capsuleDesc.motionType = MotionType::Dynamic;
		const BodyHandle capsule{ Physics::CreateBody(capsuleDesc) };

		// 落ちる箱(半分の大きさ0.5 なので止まる高さは0.5)
		BodyDesc boxDesc{};
		boxDesc.shapeData = Box{ Vector3::Zero, { 0.5f, 0.5f, 0.5f } };
		boxDesc.position = { 3.0f, 2.0f, -7.0f };
		boxDesc.motionType = MotionType::Dynamic;
		const BodyHandle box{ Physics::CreateBody(boxDesc) };

		for (int i{ 0 }; i < STEP_COUNT; i++) Physics::Step(Time::FixedDeltaTime());

		const BodyState sphereState{ Physics::GetState(sphere) };
		check("mesh: sphere rests on triangle", std::abs(sphereState.position.y - 0.5f) < POSITION_TOLERANCE);
		check("mesh: sphere stopped", std::abs(sphereState.velocity.y) < SPEED_TOLERANCE);

		const BodyState capsuleState{ Physics::GetState(capsule) };
		check("mesh: capsule rests on triangle", std::abs(capsuleState.position.y - 0.8f) < POSITION_TOLERANCE);
		check("mesh: capsule stopped", std::abs(capsuleState.velocity.y) < SPEED_TOLERANCE);

		const BodyState boxState{ Physics::GetState(box) };
		check("mesh: box rests on triangle", std::abs(boxState.position.y - 0.5f) < POSITION_TOLERANCE);
		check("mesh: box stopped", std::abs(boxState.velocity.y) < SPEED_TOLERANCE);

		// Bodyが使っている間は削除できない(エラーログが1行出るのは正しい動き) Bodyを消した後なら削除できる
		check("mesh: destroy refused while used", !Physics::DestroyTriangleMesh(mesh));
		Physics::DestroyBody(capsule);
		Physics::DestroyBody(box);
		Physics::DestroyBody(meshBody);
		Physics::DestroyBody(sphere);
		check("mesh: destroyed after body", Physics::DestroyTriangleMesh(mesh));
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
	floorDesc.friction = 0.6f;
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

	// 落ちるカプセル(球以外の形が床で止まるかを見る)
	BodyDesc fallingCapsuleDesc{};
	fallingCapsuleDesc.shapeData = Capsule{ { 0.0f, -0.5f, 0.0f }, { 0.0f, 0.5f, 0.0f }, 0.3f };
	fallingCapsuleDesc.position = { 1.5f, 4.0f, 3.0f };
	fallingCapsuleDesc.motionType = MotionType::Dynamic;
	bodies.push_back({ "Falling Capsule", fallingCapsuleDesc, { 0.3f, 0.9f, 0.9f, 1.0f } });

	// 落ちる箱
	BodyDesc fallingBoxDesc{};
	fallingBoxDesc.shapeData = Box{ Vector3::Zero, { 0.5f, 0.5f, 0.5f } };
	fallingBoxDesc.position = { -1.5f, 4.0f, 3.0f };
	fallingBoxDesc.motionType = MotionType::Dynamic;
	bodies.push_back({ "Falling Box", fallingBoxDesc, { 0.9f, 0.6f, 0.3f, 1.0f } });

	// 摩擦ありで滑る箱(床の上で止まるはず)
	BodyDesc roughBoxDesc{};
	roughBoxDesc.shapeData = Box{ Vector3::Zero, { 0.5f, 0.5f, 0.5f } };
	roughBoxDesc.position = { -4.0f, 0.5f, -2.5f };
	roughBoxDesc.motionType = MotionType::Dynamic;
	roughBoxDesc.velocity = { 3.0f, 0.0f, 0.0f };
	roughBoxDesc.friction = 0.6f;
	bodies.push_back({ "Sliding Box (friction 0.6)", roughBoxDesc, { 0.8f, 0.3f, 0.3f, 1.0f } });

	// 摩擦なしで滑る箱(同じ速さのまま進み、床の端から落ちるはず)
	BodyDesc smoothBoxDesc{ roughBoxDesc };
	smoothBoxDesc.position = { -4.0f, 0.5f, -4.0f };
	smoothBoxDesc.friction = 0.0f;
	bodies.push_back({ "Sliding Box (friction 0)", smoothBoxDesc, { 0.5f, 0.5f, 1.0f, 1.0f } });

	// 山の下に置く動かない床(山から落ちた球を受ける)
	BodyDesc tentFloorDesc{};
	tentFloorDesc.shapeData = Box{ Vector3::Zero, { 3.0f, 0.5f, 3.5f } };
	tentFloorDesc.position = { TENT_POSITION.x, -0.5f, TENT_POSITION.z };
	tentFloorDesc.motionType = MotionType::Static;
	tentFloorDesc.friction = 0.6f;
	bodies.push_back({ "Static Box (under mesh)", tentFloorDesc, { 0.6f, 0.6f, 0.6f, 1.0f } });

	// 三角形の集まりで作った動かない山
	BodyDesc tentDesc{};
	tentDesc.shapeData = TriangleMeshShape{ Physics::CreateTriangleMesh(TENT_VERTICES, TENT_INDICES) };
	tentDesc.position = TENT_POSITION;
	tentDesc.motionType = MotionType::Static;
	bodies.push_back({ "Static Mesh", tentDesc, { 0.9f, 0.9f, 0.5f, 1.0f } });

	// 左の斜面へ落ちる球(面に当たる 法線が斜面の向きを向くかを見る)
	BodyDesc faceSphereDesc{};
	faceSphereDesc.shapeData = Sphere{ Vector3::Zero, 0.4f };
	faceSphereDesc.position = { TENT_POSITION.x - 1.0f, 4.0f, -1.0f };
	faceSphereDesc.motionType = MotionType::Dynamic;
	bodies.push_back({ "Sphere on mesh face", faceSphereDesc, { 0.4f, 1.0f, 0.6f, 1.0f } });

	// 稜線の真上から落ちる球(辺に当たる)
	BodyDesc edgeSphereDesc{ faceSphereDesc };
	edgeSphereDesc.position = { TENT_POSITION.x, 4.0f, 0.5f };
	bodies.push_back({ "Sphere on mesh edge", edgeSphereDesc, { 1.0f, 0.6f, 0.4f, 1.0f } });

	// 稜線の端の少し外へ落ちる球(頂点に当たる)
	BodyDesc vertexSphereDesc{ faceSphereDesc };
	vertexSphereDesc.position = { TENT_POSITION.x, 4.0f, 2.2f };
	bodies.push_back({ "Sphere on mesh vertex", vertexSphereDesc, { 0.7f, 0.5f, 1.0f, 1.0f } });

	// 右の斜面へ落ちるカプセル
	BodyDesc meshCapsuleDesc{};
	meshCapsuleDesc.shapeData = Capsule{ { 0.0f, -0.5f, 0.0f }, { 0.0f, 0.5f, 0.0f }, 0.3f };
	meshCapsuleDesc.position = { TENT_POSITION.x + 1.0f, 4.0f, -1.0f };
	meshCapsuleDesc.motionType = MotionType::Dynamic;
	bodies.push_back({ "Capsule on mesh", meshCapsuleDesc, { 0.3f, 0.8f, 1.0f, 1.0f } });

	// 右の斜面へ落ちる箱
	BodyDesc meshBoxDesc{};
	meshBoxDesc.shapeData = Box{ Vector3::Zero, { 0.4f, 0.4f, 0.4f } };
	meshBoxDesc.position = { TENT_POSITION.x + 1.0f, 4.0f, 1.0f };
	meshBoxDesc.motionType = MotionType::Dynamic;
	bodies.push_back({ "Box on mesh", meshBoxDesc, { 1.0f, 0.9f, 0.3f, 1.0f } });

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

void DrawShape(const TriangleMeshShape& _mesh, const BodyState& _state, Vector4 _color)
{
	// 登録した中身はPhysicsから取り出せないのでテスト側で覚えている頂点と番号から三角形の辺を線で描く
	for (std::size_t i = 0; i + 2 < TENT_INDICES.size(); i += 3)
	{
		const Vector3 a{ _state.position + _state.rotation.RotateVector(TENT_VERTICES[TENT_INDICES[i]]) };
		const Vector3 b{ _state.position + _state.rotation.RotateVector(TENT_VERTICES[TENT_INDICES[i + 1]]) };
		const Vector3 c{ _state.position + _state.rotation.RotateVector(TENT_VERTICES[TENT_INDICES[i + 2]]) };
		Gfx::DrawLine3D(a, b, _color);
		Gfx::DrawLine3D(b, c, _color);
		Gfx::DrawLine3D(c, a, _color);
	}
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
