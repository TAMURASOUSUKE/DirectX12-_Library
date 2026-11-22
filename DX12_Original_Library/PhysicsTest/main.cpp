#include "../Src/Facade/TSLib.h"
#include <algorithm>
#include <chrono>
#include <cmath>
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
	double largeFieldStepMilliseconds{ 0.0 }; // 広い高さの格子の上でのStep一回の平均時間
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

// 画面に置く高さの格子(点は5x5 升は一辺2 描画にも使うのでテスト側で覚える)
constexpr int FIELD_COUNT_X{ 5 };
constexpr int FIELD_COUNT_Z{ 5 };
constexpr float FIELD_CELL_SIZE{ 2.0f };
// 高さの並び heights[z * countX + x] 手前が低く奥へ向かって上がる坂と中央のくぼみ
const std::vector<float> FIELD_HEIGHTS{
	0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
	0.0f, 0.2f, 0.0f, 0.2f, 0.0f,
	0.3f, 0.5f, 0.2f, 0.5f, 0.3f,
	0.8f, 1.0f, 0.8f, 1.0f, 0.8f,
	1.5f, 1.5f, 1.5f, 1.5f, 1.5f,
};
// 高さの格子を置く位置(格子の角)
const Vector3 FIELD_POSITION{ -15.5f, 0.0f, -4.0f };

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

	// 高さの格子 登録できること と 球・カプセル・箱が平らな格子の上で止まること と 格子の外では当たらないこと
	// この項目と期待値はAIが置いた(形の大きさから機械的に決まる確認だけ)
	{
		constexpr int STEP_COUNT{ 180 }; // 3秒分進める
		constexpr float POSITION_TOLERANCE{ 0.03f }; // 止まる高さの許す誤差
		constexpr float SPEED_TOLERANCE{ 0.05f }; // 止まったとみなす速さ

		// 点は3x3 升は一辺4 高さは全部0 の平らな格子(一辺8)
		const std::vector<float> heights(9, 0.0f);
		const HeightFieldHandle field{ Physics::CreateHeightField(3, 3, 4.0f, heights) };
		check("field: registered", field.IsValid());
		// 高さの数が合わない並びは登録できない(エラーログが1行出るのは正しい動き)
		check("field: wrong count rejected", !Physics::CreateHeightField(4, 3, 4.0f, heights).IsValid());

		BodyDesc fieldDesc{};
		fieldDesc.shapeData = HeightFieldShape{ field };
		fieldDesc.position = { 20.0f, 0.0f, 20.0f }; // 格子の角
		fieldDesc.motionType = MotionType::Static;
		const BodyHandle fieldBody{ Physics::CreateBody(fieldDesc) };
		check("field: static body created", fieldBody.IsValid());

		// 落ちる球(半径0.5 なので止まる高さは0.5 升の境目と対角線から離れた位置へ落とす)
		BodyDesc sphereDesc{};
		sphereDesc.shapeData = Sphere{ Vector3::Zero, 0.5f };
		sphereDesc.position = { 26.5f, 2.0f, 21.0f };
		sphereDesc.motionType = MotionType::Dynamic;
		const BodyHandle sphere{ Physics::CreateBody(sphereDesc) };

		// 落ちるカプセル(止まる高さは0.8)
		BodyDesc capsuleDesc{};
		capsuleDesc.shapeData = Capsule{ { 0.0f, -0.5f, 0.0f }, { 0.0f, 0.5f, 0.0f }, 0.3f };
		capsuleDesc.position = { 22.5f, 2.0f, 21.0f };
		capsuleDesc.motionType = MotionType::Dynamic;
		const BodyHandle capsule{ Physics::CreateBody(capsuleDesc) };

		// 落ちる箱(止まる高さは0.5)
		BodyDesc boxDesc{};
		boxDesc.shapeData = Box{ Vector3::Zero, { 0.5f, 0.5f, 0.5f } };
		boxDesc.position = { 26.5f, 2.0f, 25.0f };
		boxDesc.motionType = MotionType::Dynamic;
		const BodyHandle box{ Physics::CreateBody(boxDesc) };

		// 升の境目の真上へ落とす球(格子のローカルで x=4 の線の上 境目で落ちないことを見る)
		BodyDesc borderDesc{ sphereDesc };
		borderDesc.position = { 24.0f, 2.0f, 22.5f };
		const BodyHandle border{ Physics::CreateBody(borderDesc) };

		// 格子の外へ落とす球(受ける物が無いので落ち続けるはず)
		BodyDesc outsideDesc{ sphereDesc };
		outsideDesc.position = { 17.0f, 2.0f, 21.0f };
		const BodyHandle outside{ Physics::CreateBody(outsideDesc) };

		for (int i{ 0 }; i < STEP_COUNT; i++) Physics::Step(Time::FixedDeltaTime());

		const BodyState sphereState{ Physics::GetState(sphere) };
		check("field: sphere rests on field", std::abs(sphereState.position.y - 0.5f) < POSITION_TOLERANCE);
		check("field: sphere stopped", std::abs(sphereState.velocity.y) < SPEED_TOLERANCE);

		const BodyState capsuleState{ Physics::GetState(capsule) };
		check("field: capsule rests on field", std::abs(capsuleState.position.y - 0.8f) < POSITION_TOLERANCE);

		const BodyState boxState{ Physics::GetState(box) };
		check("field: box rests on field", std::abs(boxState.position.y - 0.5f) < POSITION_TOLERANCE);

		check("field: sphere stays on cell border", std::abs(Physics::GetPosition(border).y - 0.5f) < POSITION_TOLERANCE);
		check("field: no contact outside", Physics::GetPosition(outside).y < -1.0f);

		// Bodyが使っている間は削除できない(エラーログが1行出るのは正しい動き) Bodyを消した後なら削除できる
		check("field: destroy refused while used", !Physics::DestroyHeightField(field));
		Physics::DestroyBody(sphere);
		Physics::DestroyBody(capsule);
		Physics::DestroyBody(box);
		Physics::DestroyBody(border);
		Physics::DestroyBody(outside);
		Physics::DestroyBody(fieldBody);
		check("field: destroyed after body", Physics::DestroyHeightField(field));
	}

	// 広い高さの格子(点は129x129 升は一辺16 一辺2048)の上でも球が止まること と そのときのStepの時間
	// 時間は画面に出すだけで合否には使わない(基準を決めるのは本人)
	{
		constexpr int STEP_COUNT{ 180 }; // 3秒分進める
		constexpr float POSITION_TOLERANCE{ 0.03f }; // 止まる高さの許す誤差
		constexpr int LARGE_COUNT{ 129 }; // 一辺の点の数

		const std::vector<float> heights(static_cast<std::size_t>(LARGE_COUNT) * LARGE_COUNT, 0.0f);
		const HeightFieldHandle field{ Physics::CreateHeightField(LARGE_COUNT, LARGE_COUNT, 16.0f, heights) };
		check("large field: registered", field.IsValid());

		BodyDesc fieldDesc{};
		fieldDesc.shapeData = HeightFieldShape{ field };
		fieldDesc.position = { 1000.0f, 0.0f, 1000.0f }; // 格子の角
		fieldDesc.motionType = MotionType::Static;
		const BodyHandle fieldBody{ Physics::CreateBody(fieldDesc) };

		// 格子の中ほどへ落とす球(升の境目と対角線から離れた位置)
		BodyDesc sphereDesc{};
		sphereDesc.shapeData = Sphere{ Vector3::Zero, 0.5f };
		sphereDesc.position = { 2001.0f, 2.0f, 1500.0f };
		sphereDesc.motionType = MotionType::Dynamic;
		const BodyHandle sphere{ Physics::CreateBody(sphereDesc) };

		const auto startTime{ std::chrono::steady_clock::now() };
		for (int i{ 0 }; i < STEP_COUNT; i++) Physics::Step(Time::FixedDeltaTime());
		const std::chrono::duration<double, std::milli> elapsed{ std::chrono::steady_clock::now() - startTime };
		result.largeFieldStepMilliseconds = elapsed.count() / static_cast<double>(STEP_COUNT);

		check("large field: sphere rests", std::abs(Physics::GetPosition(sphere).y - 0.5f) < POSITION_TOLERANCE);

		Physics::DestroyBody(sphere);
		Physics::DestroyBody(fieldBody);
		check("large field: destroyed", Physics::DestroyHeightField(field));
	}

	// レイヤーであたり判定の相手を絞る(#162)
	// 項目は本人が決めた4つ : 外した組は接触の一覧に出ない / 片方だけ外しても出ない / 何も設定しない同士は今まで通り当たる / 三角形の集まりと高さの格子でも効く
	// 組み立てはAIが書いた 床の上へ球を落とし Stepごとの接触の数の最大と 最後の高さを見る(当たるなら止まる 当たらないならすり抜けて落ち続ける)
	{
		constexpr int STEP_COUNT{ 180 }; // 3秒分進める
		constexpr float POSITION_TOLERANCE{ 0.03f }; // 止まる高さの許す誤差
		constexpr float REST_HEIGHT{ 0.5f }; // 半径0.5の球が y=0 の面で止まる高さ
		constexpr float FALLEN_HEIGHT{ -1.0f }; // これより下ならすり抜けたとみなす
		constexpr std::uint32_t LAYER_GROUND{ 1u << 1 }; // 床に付けるレイヤー
		constexpr std::uint32_t LAYER_BALL{ 1u << 2 }; // 球に付けるレイヤー
		constexpr std::uint32_t MASK_ALL{ 0xFFFFFFFF }; // 全てのレイヤーを相手にする

		// 球を落とした結果
		struct DropOutcome
		{
			int maxContactCount{ 0 }; // Stepごとの接触の数の最大(一度も当たらなければ0)
			float finalHeight{ 0.0f }; // 最後の球の高さ
		};
		// 床を一つ作り その上へ球を一つ落として結果を返す(作ったBodyはここで消す)
		const auto dropSphere = [STEP_COUNT](const BodyDesc& _floorDesc, Vector3 _spherePosition, std::uint32_t _sphereLayer, std::uint32_t _sphereMask)
			{
				const BodyHandle floorBody{ Physics::CreateBody(_floorDesc) };

				BodyDesc sphereDesc{};
				sphereDesc.shapeData = Sphere{ Vector3::Zero, 0.5f };
				sphereDesc.position = _spherePosition;
				sphereDesc.motionType = MotionType::Dynamic;
				sphereDesc.layer = _sphereLayer;
				sphereDesc.collisionMask = _sphereMask;
				const BodyHandle sphere{ Physics::CreateBody(sphereDesc) };

				DropOutcome outcome{};
				for (int i{ 0 }; i < STEP_COUNT; i++)
				{
					Physics::Step(Time::FixedDeltaTime());
					outcome.maxContactCount = (std::max)(outcome.maxContactCount, static_cast<int>(Physics::GetContacts().size()));
				}
				outcome.finalHeight = Physics::GetPosition(sphere).y;

				Physics::DestroyBody(sphere);
				Physics::DestroyBody(floorBody);
				return outcome;
			};

		// 箱の床(上面は y=0)
		BodyDesc boxFloorDesc{};
		boxFloorDesc.shapeData = Box{ Vector3::Zero, { 10.0f, 0.5f, 10.0f } };
		boxFloorDesc.position = { 0.0f, -0.5f, 0.0f };
		boxFloorDesc.motionType = MotionType::Static;
		const Vector3 boxDropPosition{ 0.0f, 2.0f, 0.0f };

		// 何も設定しない同士は今まで通り当たる(layerもcollisionMaskも初期値のまま)
		{
			const BodyDesc defaultSphere{};
			const DropOutcome outcome{ dropSphere(boxFloorDesc, boxDropPosition, defaultSphere.layer, defaultSphere.collisionMask) };
			check("layer: default pair has contact", outcome.maxContactCount > 0);
			check("layer: default pair rests", std::abs(outcome.finalHeight - REST_HEIGHT) < POSITION_TOLERANCE);
		}

		// レイヤーを付けても 互いに相手を含んでいれば当たる
		{
			BodyDesc floorDesc{ boxFloorDesc };
			floorDesc.layer = LAYER_GROUND;
			floorDesc.collisionMask = LAYER_BALL;
			const DropOutcome outcome{ dropSphere(floorDesc, boxDropPosition, LAYER_BALL, LAYER_GROUND) };
			check("layer: matched pair has contact", outcome.maxContactCount > 0);
			check("layer: matched pair rests", std::abs(outcome.finalHeight - REST_HEIGHT) < POSITION_TOLERANCE);
		}

		// 両方が相手のレイヤーを外した組は接触の一覧に出ない
		{
			BodyDesc floorDesc{ boxFloorDesc };
			floorDesc.layer = LAYER_GROUND;
			floorDesc.collisionMask = MASK_ALL & ~LAYER_BALL;
			const DropOutcome outcome{ dropSphere(floorDesc, boxDropPosition, LAYER_BALL, MASK_ALL & ~LAYER_GROUND) };
			check("layer: both excluded has no contact", outcome.maxContactCount == 0);
			check("layer: both excluded falls through", outcome.finalHeight < FALLEN_HEIGHT);
		}

		// 球だけが床のレイヤーを外しても出ない(床は全てを相手にしている)
		{
			BodyDesc floorDesc{ boxFloorDesc };
			floorDesc.layer = LAYER_GROUND;
			floorDesc.collisionMask = MASK_ALL;
			const DropOutcome outcome{ dropSphere(floorDesc, boxDropPosition, LAYER_BALL, MASK_ALL & ~LAYER_GROUND) };
			check("layer: only sphere excludes has no contact", outcome.maxContactCount == 0);
			check("layer: only sphere excludes falls through", outcome.finalHeight < FALLEN_HEIGHT);
		}

		// 床だけが球のレイヤーを外しても出ない(球は全てを相手にしている)
		{
			BodyDesc floorDesc{ boxFloorDesc };
			floorDesc.layer = LAYER_GROUND;
			floorDesc.collisionMask = MASK_ALL & ~LAYER_BALL;
			const DropOutcome outcome{ dropSphere(floorDesc, boxDropPosition, LAYER_BALL, MASK_ALL) };
			check("layer: only floor excludes has no contact", outcome.maxContactCount == 0);
			check("layer: only floor excludes falls through", outcome.finalHeight < FALLEN_HEIGHT);
		}

		// 三角形の集まりでも効く(一辺20の平らな四角 面は y=0 対角線から離れた位置へ落とす)
		{
			const std::vector<Vector3> vertices{ { -10.0f, 0.0f, -10.0f }, { -10.0f, 0.0f, 10.0f }, { 10.0f, 0.0f, 10.0f }, { 10.0f, 0.0f, -10.0f } };
			const std::vector<std::uint32_t> indices{ 0, 1, 2,  0, 2, 3 };
			const TriangleMeshHandle mesh{ Physics::CreateTriangleMesh(vertices, indices) };
			BodyDesc meshDesc{};
			meshDesc.shapeData = TriangleMeshShape{ mesh };
			meshDesc.motionType = MotionType::Static;
			meshDesc.layer = LAYER_GROUND;
			const Vector3 meshDropPosition{ 3.0f, 2.0f, -3.0f };

			const DropOutcome matched{ dropSphere(meshDesc, meshDropPosition, LAYER_BALL, MASK_ALL) };
			check("layer: mesh matched has contact", matched.maxContactCount > 0);
			check("layer: mesh matched rests", std::abs(matched.finalHeight - REST_HEIGHT) < POSITION_TOLERANCE);

			const DropOutcome excluded{ dropSphere(meshDesc, meshDropPosition, LAYER_BALL, MASK_ALL & ~LAYER_GROUND) };
			check("layer: mesh excluded has no contact", excluded.maxContactCount == 0);
			check("layer: mesh excluded falls through", excluded.finalHeight < FALLEN_HEIGHT);

			check("layer: mesh destroyed", Physics::DestroyTriangleMesh(mesh));
		}

		// 高さの格子でも効く(点は3x3 升は一辺4 高さは全部0 升の境目と対角線から離れた位置へ落とす)
		{
			const std::vector<float> heights(9, 0.0f);
			const HeightFieldHandle field{ Physics::CreateHeightField(3, 3, 4.0f, heights) };
			BodyDesc fieldDesc{};
			fieldDesc.shapeData = HeightFieldShape{ field };
			fieldDesc.position = { 20.0f, 0.0f, 20.0f }; // 格子の角
			fieldDesc.motionType = MotionType::Static;
			fieldDesc.layer = LAYER_GROUND;
			const Vector3 fieldDropPosition{ 26.5f, 2.0f, 21.0f };

			const DropOutcome matched{ dropSphere(fieldDesc, fieldDropPosition, LAYER_BALL, MASK_ALL) };
			check("layer: field matched has contact", matched.maxContactCount > 0);
			check("layer: field matched rests", std::abs(matched.finalHeight - REST_HEIGHT) < POSITION_TOLERANCE);

			const DropOutcome excluded{ dropSphere(fieldDesc, fieldDropPosition, LAYER_BALL, MASK_ALL & ~LAYER_GROUND) };
			check("layer: field excluded has no contact", excluded.maxContactCount == 0);
			check("layer: field excluded falls through", excluded.finalHeight < FALLEN_HEIGHT);

			check("layer: field destroyed", Physics::DestroyHeightField(field));
		}
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


	// 高さの格子で作った動かない坂
	BodyDesc fieldDesc{};
	fieldDesc.shapeData = HeightFieldShape{ Physics::CreateHeightField(FIELD_COUNT_X, FIELD_COUNT_Z, FIELD_CELL_SIZE, FIELD_HEIGHTS) };
	fieldDesc.position = FIELD_POSITION;
	fieldDesc.motionType = MotionType::Static;
	fieldDesc.friction = 0.6f;
	bodies.push_back({ "Static Height Field", fieldDesc, { 0.5f, 0.9f, 0.6f, 1.0f } });

	// 坂の上へ落ちる球
	BodyDesc fieldSphereDesc{};
	fieldSphereDesc.shapeData = Sphere{ Vector3::Zero, 0.4f };
	fieldSphereDesc.position = { FIELD_POSITION.x + 3.0f, 4.0f, FIELD_POSITION.z + 6.5f };
	fieldSphereDesc.motionType = MotionType::Dynamic;
	bodies.push_back({ "Sphere on field", fieldSphereDesc, { 0.4f, 1.0f, 0.6f, 1.0f } });

	// 坂の上へ落ちるカプセル
	BodyDesc fieldCapsuleDesc{};
	fieldCapsuleDesc.shapeData = Capsule{ { 0.0f, -0.5f, 0.0f }, { 0.0f, 0.5f, 0.0f }, 0.3f };
	fieldCapsuleDesc.position = { FIELD_POSITION.x + 5.0f, 4.0f, FIELD_POSITION.z + 5.0f };
	fieldCapsuleDesc.motionType = MotionType::Dynamic;
	bodies.push_back({ "Capsule on field", fieldCapsuleDesc, { 0.3f, 0.8f, 1.0f, 1.0f } });

	// 平らな所へ落ちる箱
	BodyDesc fieldBoxDesc{};
	fieldBoxDesc.shapeData = Box{ Vector3::Zero, { 0.4f, 0.4f, 0.4f } };
	fieldBoxDesc.position = { FIELD_POSITION.x + 6.9f, 4.0f, FIELD_POSITION.z + 1.0f };
	fieldBoxDesc.motionType = MotionType::Dynamic;
	bodies.push_back({ "Box on field", fieldBoxDesc, { 1.0f, 0.9f, 0.3f, 1.0f } });

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

void DrawShape(const HeightFieldShape& _heightField, const BodyState& _state, Vector4 _color)
{
	// 登録した中身はPhysicsから取り出せないのでテスト側で覚えている高さから升の辺と対角線を線で描く
	const auto corner = [&_state](int _x, int _z)
		{
			const Vector3 local{ static_cast<float>(_x) * FIELD_CELL_SIZE, FIELD_HEIGHTS[_z * FIELD_COUNT_X + _x], static_cast<float>(_z) * FIELD_CELL_SIZE };
			return _state.position + _state.rotation.RotateVector(local);
		};
	for (int z = 0; z < FIELD_COUNT_Z; z++)
	{
		for (int x = 0; x < FIELD_COUNT_X; x++)
		{
			if (x + 1 < FIELD_COUNT_X) Gfx::DrawLine3D(corner(x, z), corner(x + 1, z), _color);
			if (z + 1 < FIELD_COUNT_Z) Gfx::DrawLine3D(corner(x, z), corner(x, z + 1), _color);
			if (x + 1 < FIELD_COUNT_X && z + 1 < FIELD_COUNT_Z) Gfx::DrawLine3D(corner(x, z), corner(x + 1, z + 1), _color);
		}
	}
}

// 形がXとZの向きに占める範囲(F7の升の表示で使う ライブラリの中と同じ計算をテスト側でやり直している)
struct TestRangeXZ
{
	bool isValid{ false }; // 範囲を持つ形か(動かない形は持たない)
	float minX{ 0.0f }; // Xの最小
	float maxX{ 0.0f }; // Xの最大
	float minZ{ 0.0f }; // Zの最小
	float maxZ{ 0.0f }; // Zの最大
};

TestRangeXZ GetTestRangeXZ(const Sphere& _sphere, const BodyState& _state)
{
	const Vector3 center{ _state.position + _state.rotation.RotateVector(_sphere.center) };
	return TestRangeXZ{ true, center.x - _sphere.radius, center.x + _sphere.radius, center.z - _sphere.radius, center.z + _sphere.radius };
}

TestRangeXZ GetTestRangeXZ(const Capsule& _capsule, const BodyState& _state)
{
	const Vector3 start{ _state.position + _state.rotation.RotateVector(_capsule.startPos) };
	const Vector3 end{ _state.position + _state.rotation.RotateVector(_capsule.endPos) };
	return TestRangeXZ{ true,
		(std::min)(start.x, end.x) - _capsule.radius, (std::max)(start.x, end.x) + _capsule.radius,
		(std::min)(start.z, end.z) - _capsule.radius, (std::max)(start.z, end.z) + _capsule.radius };
}

TestRangeXZ GetTestRangeXZ(const Box& _box, const BodyState& _state)
{
	const Vector3 center{ _state.position + _state.rotation.RotateVector(_box.center) };
	const Quaternion rotation{ _state.rotation * _box.rotation };
	const Vector3 axisX{ rotation.RotateVector(Vector3::Right) };
	const Vector3 axisY{ rotation.RotateVector(Vector3::Up) };
	const Vector3 axisZ{ rotation.RotateVector(Vector3::Forward) };
	const float radiusX{ _box.halfSize.x * std::abs(axisX.x) + _box.halfSize.y * std::abs(axisY.x) + _box.halfSize.z * std::abs(axisZ.x) };
	const float radiusZ{ _box.halfSize.x * std::abs(axisX.z) + _box.halfSize.y * std::abs(axisY.z) + _box.halfSize.z * std::abs(axisZ.z) };
	return TestRangeXZ{ true, center.x - radiusX, center.x + radiusX, center.z - radiusZ, center.z + radiusZ };
}

TestRangeXZ GetTestRangeXZ(const TriangleMeshShape& _mesh, const BodyState& _state)
{
	return TestRangeXZ{};
}

TestRangeXZ GetTestRangeXZ(const HeightFieldShape& _heightField, const BodyState& _state)
{
	return TestRangeXZ{};
}

// 画面の高さの格子の上で 形の範囲が重なっている升を枠と対角線で描く(格子は回転させずに置いている前提)
void DrawOverlappedFieldCells(const TestRangeXZ& _range, Vector4 _color)
{
	if (!_range.isValid) return;

	// ワールドの範囲を格子の角からの位置へ直す
	const float minX{ _range.minX - FIELD_POSITION.x };
	const float maxX{ _range.maxX - FIELD_POSITION.x };
	const float minZ{ _range.minZ - FIELD_POSITION.z };
	const float maxZ{ _range.maxZ - FIELD_POSITION.z };
	const float fieldSizeX{ static_cast<float>(FIELD_COUNT_X - 1) * FIELD_CELL_SIZE };
	const float fieldSizeZ{ static_cast<float>(FIELD_COUNT_Z - 1) * FIELD_CELL_SIZE };
	if (maxX < 0.0f || minX > fieldSizeX || maxZ < 0.0f || minZ > fieldSizeZ) return; // 格子の外

	// 位置を升の大きさで割って切り捨てると升の番号になる
	const int cellMinX{ std::clamp(static_cast<int>(std::floor(minX / FIELD_CELL_SIZE)), 0, FIELD_COUNT_X - 2) };
	const int cellMaxX{ std::clamp(static_cast<int>(std::floor(maxX / FIELD_CELL_SIZE)), 0, FIELD_COUNT_X - 2) };
	const int cellMinZ{ std::clamp(static_cast<int>(std::floor(minZ / FIELD_CELL_SIZE)), 0, FIELD_COUNT_Z - 2) };
	const int cellMaxZ{ std::clamp(static_cast<int>(std::floor(maxZ / FIELD_CELL_SIZE)), 0, FIELD_COUNT_Z - 2) };

	constexpr float LIFT{ 0.03f }; // 格子の線と重ならないよう少し浮かせる
	const auto corner = [LIFT](int _x, int _z)
		{
			return FIELD_POSITION + Vector3{ static_cast<float>(_x) * FIELD_CELL_SIZE, FIELD_HEIGHTS[_z * FIELD_COUNT_X + _x] + LIFT, static_cast<float>(_z) * FIELD_CELL_SIZE };
		};
	for (int z = cellMinZ; z <= cellMaxZ; z++)
	{
		for (int x = cellMinX; x <= cellMaxX; x++)
		{
			const Vector3 corner00{ corner(x, z) };
			const Vector3 corner10{ corner(x + 1, z) };
			const Vector3 corner01{ corner(x, z + 1) };
			const Vector3 corner11{ corner(x + 1, z + 1) };
			Gfx::DrawLine3D(corner00, corner10, _color);
			Gfx::DrawLine3D(corner10, corner11, _color);
			Gfx::DrawLine3D(corner11, corner01, _color);
			Gfx::DrawLine3D(corner01, corner00, _color);
			Gfx::DrawLine3D(corner00, corner11, _color); // 三角形2枚に割る対角線
		}
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
	bool isFieldCellVisible{ false }; // 高さの格子で調べている升を描くか
	int stepCount{ 0 }; // 作り直してからStepを呼んだ回数
	double lastStepMilliseconds{ 0.0 }; // 直近のStep一回の時間
	double maxStepMilliseconds{ 0.0 }; // 作り直してからの最大
	int lastContactCount{ -1 }; // 前のフレームの接触の数(変わったときだけログへ出す)
	constexpr float CONTACT_POINT_RADIUS{ 0.06f }; // 接触の点を表す球の半径
	constexpr float CONTACT_NORMAL_LENGTH{ 0.6f }; // 法線を表す線の長さ

	// 注視点の周りを回るカメラ(注視点はキーで動かせる)
	Camera camera{};
	float cameraYaw{ 0.0f };
	float cameraPitch{ 15.0f * Math::DEG_TO_RAD };
	Vector3 cameraTarget{ 0.0f, 2.0f, 0.0f };
	constexpr float CAMERA_DISTANCE{ 14.0f };
	constexpr float CAMERA_MOVE_SPEED{ 8.0f }; // 注視点を動かす速さ(1秒あたり)
	constexpr float CAMERA_FAST_MOVE_SPEED{ 24.0f }; // Shiftを押している間の速さ
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

		// W A S D : 注視点を水平に動かす  E Q : 上下に動かす  Shift : 速く動かす
		{
			// カメラの左右の向きだけを使い 見上げていても水平に進むようにする
			const Quaternion yawRotation{ Quaternion::FromEuler(0.0f, cameraYaw, 0.0f) };
			const Vector3 moveForward{ yawRotation.RotateVector(Vector3::Forward) };
			const Vector3 moveRight{ yawRotation.RotateVector(Vector3::Right) };

			Vector3 moveDirection{ Vector3::Zero };
			if (Input::IsKeyPress(KeyCode::Button::W)) moveDirection += moveForward;
			if (Input::IsKeyPress(KeyCode::Button::S)) moveDirection -= moveForward;
			if (Input::IsKeyPress(KeyCode::Button::D)) moveDirection += moveRight;
			if (Input::IsKeyPress(KeyCode::Button::A)) moveDirection -= moveRight;
			if (Input::IsKeyPress(KeyCode::Button::E)) moveDirection += Vector3::Up;
			if (Input::IsKeyPress(KeyCode::Button::Q)) moveDirection -= Vector3::Up;

			const float moveSpeed{ Input::IsKeyPress(KeyCode::Button::SHIFT) ? CAMERA_FAST_MOVE_SPEED : CAMERA_MOVE_SPEED };
			cameraTarget += moveDirection * (moveSpeed * Time::DeltaTime());
		}

		// Space : 一時停止の切り替え
		if (Input::IsKeyPushed(KeyCode::Button::SPACE)) isPaused = !isPaused;
		// N : 一時停止中に一回だけ進める
		bool isStepOnceRequested{ isPaused && Input::IsKeyPushed(KeyCode::Button::N) };
		// F1 : Debug Overlayの切り替え
		if (Input::IsKeyPushed(KeyCode::Button::F1)) Debug::SetOverlayVisible(!Debug::IsOverlayVisible());
		// F7 : 高さの格子で調べている升の表示の切り替え
		if (Input::IsKeyPushed(KeyCode::Button::F7)) isFieldCellVisible = !isFieldCellVisible;
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

		// F7 : 形の範囲が重なっている升(高さの格子が実際に調べる升)を赤い枠で描く
		if (isFieldCellVisible)
		{
			for (const TestBody& body : bodies)
			{
				const BodyState state{ Physics::GetState(body.handle) };
				const TestRangeXZ range{ std::visit([&state](const auto& _shape) { return GetTestRangeXZ(_shape, state); }, body.desc.shapeData) };
				DrawOverlappedFieldCells(range, { 1.0f, 0.2f, 0.2f, 1.0f });
			}
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
		Gfx::DrawString("Space: pause  N: step once  R: recreate  F1: overlay  Right drag: rotate  WASD/QE: move  Shift: fast  F7: field cells  ESC: exit", { 20.0f, textY });
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
		textY += LINE_HEIGHT;

		// 広い高さの格子の上でのStep一回の平均時間(起動時に測った値)
		std::snprintf(text, sizeof(text), "Large height field (129x129)  Step avg: %.4f ms", tests.largeFieldStepMilliseconds);
		Gfx::DrawString(text, { 20.0f, textY });

		TSLib::EndFrame();
	}

	DestroyBodies(bodies);
	TSLib::Finish();

	// 数値テストが失敗した場合は終了コードも失敗にする
	return tests.failedCount == 0 ? 0 : 1;
}
