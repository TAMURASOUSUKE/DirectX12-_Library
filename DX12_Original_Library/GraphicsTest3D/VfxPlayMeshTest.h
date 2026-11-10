#pragma once
#include <array>
#include <vector>
#include <cstdint>
#include <cmath>
#include "../Src/Facade/TSLib.h"

// VFX05の残り（板と帯の再生管理）のテスト
// 同じシェーダー1本（VfxPlayMeshTestPS）をパラメータだけ変えて、板3種・帯1種・Material数の確認用1種を出す
// 使う公開API（ライブラリ側）:
//   Gfx::SetMaterialParameter(material, 0, LookParameter)   b0 space1 に種類ごとの見た目を置く（テンプレートの値はSlotごとの複製へ引き継がれる）
//   Gfx::RegisterVFXMesh(VFXMeshDesc)                       板か帯の種類を登録し、maxInstancesぶんのSlot（Materialの複製 帯なら点の置き場）を作る
//   Gfx::PlayVFX(meshType, position, rotation)              空いているSlotを1つ借りて再生開始
//   Gfx::SetVFXTrailPoint(handle, left, right)              帯へこのフレームの左右の点を1組渡す
//   Gfx::UpdateVFX(handle, dt)                              経過を進め、時間をb3 space1へ書き、描画登録を行う。終了した更新で false
//   Gfx::StopVFX(handle, false)                             帯は点の受け付けだけ止まり、残りは寿命で消える
// 見て確かめること:
//   1. 板が時間で溶けて消える（progress が b3 space1 に届いている）
//   2. 同じ種類を重ねて再生しても、1枚ずつ別の進み具合で溶ける（Slotごとに時間を持っている）
//   3. 板の向きが FaceCamera / AxisY / None で変わる
//   4. 3つ目の板（None）は maxInstances が2なので、3つ目の再生が断られてログに出る
//   5. 帯は古い端が薄く、受け付けが終わったあとも残りが寿命で消える。2回に1回は途中で StopVFX して同じ消え方になる
//   6. 格子の64枚が順番に溶ける（Materialを64個より多く作れている）
struct VfxPlayMeshTest
{
	// VfxPlayMeshTestPS.hlsl の cbuffer と同じ並び（SetMaterialParameter slot0）
	struct alignas(16) LookParameter
	{
		float baseColor[4]{ 1.0f, 1.0f, 1.0f, 1.0f };
		float flowSpeed{ 0.0f };   // 模様が横へ流れる速さ（uv/秒）
		float edgeWidth{ 0.05f };  // 溶ける境目のぼかし幅
		float dissolve{ 1.0f };    // 1で progress に合わせて溶ける 0で溶けない（帯用）
		float lookPadding{ 0.0f };
	};
	static_assert(sizeof(LookParameter) == 32, "LookParameter must match the HLSL cbuffer (32 bytes)");

	// 見る位置に合わせて調整する
	Vector3 center{ 0.0f, 50.0f, 0.0f }; // テストの中心

	// 板1種類ぶんの登録と、その再生の様子
	struct QuadKind
	{
		const char* name{ "" };
		MaterialBlendMode blendMode{ MaterialBlendMode::Blend };
		LookParameter look{};
		VFXMeshDesc desc{};               // material はテンプレート。ライブラリがSlotごとに複製する
		float replayInterval{ 1.0f };     // この秒数ごとに新しく再生開始する
		Vector3 offset{ Vector3::Zero };  // center からの位置
		Vector3 step{ Vector3::Zero };    // 再生するたびにずらす量（重なった再生を見分けるため）
		std::uint32_t stepCount{ 1 };     // 何回ずらしたら元の位置へ戻るか
		Quaternion rotation{ Quaternion::Identity };

		VFXMeshTypeHandle meshType{};
		std::vector<VFXHandle> playing{}; // 再生中のハンドル。終了したものは毎フレーム外す
		float replayTimer{ 0.0f };
		std::uint32_t playCount{ 0 };     // 再生開始に成功した回数
		std::uint32_t rejectedCount{ 0 }; // 空きSlotが無くて断られた回数
	};

	// 再生中の帯1本ぶん
	struct TrailPlaying
	{
		VFXHandle handle{};
		float angle{ 0.0f };          // 剣の角度
		float age{ 0.0f };            // 再生開始からの秒数
		bool stopAtHalf{ false };     // 受け付けの途中で StopVFX するか
		bool stopRequested{ false };  // もう StopVFX を呼んだか
	};

	static constexpr std::uint32_t GRID_SIDE{ 8 };                       // 格子の1辺の枚数
	static constexpr std::uint32_t GRID_COUNT{ GRID_SIDE * GRID_SIDE };  // 格子の枚数 = Materialの複製の数

	ShaderHandle pixelShader{};
	std::array<QuadKind, 3> quadKinds{};

	// 帯
	MaterialHandle trailMaterial{};
	VFXMeshDesc trailDesc{};
	VFXMeshTypeHandle trailType{};
	std::vector<TrailPlaying> trails{};
	float trailReplayTimer{ 0.0f };
	float trailReplayInterval{ 2.0f };
	float sweepRadiusRoot{ 1.0f }; // 剣の根元の半径
	float sweepRadiusTip{ 3.0f };  // 剣先の半径
	float sweepSpeed{ 6.0f };      // 1秒あたりの回転(ラジアン)
	std::uint32_t trailPlayCount{ 0 };

	// Material数の確認用の格子
	QuadKind grid{};
	std::uint32_t gridNext{ 0 }; // 次に再生する格子の番号

	bool Setup()
	{
		pixelShader = Gfx::LoadShader(L"VfxPlayMeshTestPS.hlsl", ShaderUsage::VFX, ShaderStage::Pixel);
		if (!pixelShader.IsValid()) return false;

		SetupFaceCamera(quadKinds[0]);
		SetupAxisY(quadKinds[1]);
		SetupNone(quadKinds[2]);
		for (QuadKind& kind : quadKinds)
		{
			if (!RegisterQuadKind(kind)) return false;
		}

		if (!RegisterTrail()) return false;

		// 格子は登録に失敗してもほかのテストを止めない（MAX_MATERIAL_COUNT が足りないとここで失敗する）
		SetupGrid(grid);
		if (RegisterQuadKind(grid)) DEBUG_LOG("VFXメッシュの格子を登録しました Materialの複製{}個\n", GRID_COUNT);
		return true;
	}

	void Shutdown()
	{
		for (QuadKind& kind : quadKinds) ShutdownQuadKind(kind);
		ShutdownQuadKind(grid);

		for (TrailPlaying& trail : trails) Gfx::StopVFX(trail.handle, true);
		trails.clear();
		Gfx::Unload(trailMaterial);

		Gfx::Unload(pixelShader);
	}

	// 毎フレーム：一定間隔で再生開始 → 再生中のものを更新 → 終了したものを外す
	// 描画登録は UpdateVFX の中でライブラリが行うので、Draw は無い
	void Update(float _deltaTime)
	{
		for (QuadKind& kind : quadKinds) UpdateQuadKind(kind, _deltaTime);
		UpdateTrails(_deltaTime);
		if (grid.meshType.IsValid()) UpdateGrid(_deltaTime);
	}

private:
	// カメラ正対・加算。カメラをどう回しても正面を向く。3枚が重なりながら別々に溶ける
	static void SetupFaceCamera(QuadKind& _kind)
	{
		_kind.name = "FaceCamera";
		_kind.blendMode = MaterialBlendMode::Additive;
		_kind.look.baseColor[0] = 0.2f; _kind.look.baseColor[1] = 0.8f; _kind.look.baseColor[2] = 1.0f; _kind.look.baseColor[3] = 0.9f;
		_kind.look.flowSpeed = 0.5f;
		_kind.desc.billboardMode = BillboardMode::FaceCamera;
		_kind.desc.width = 2.0f;
		_kind.desc.height = 2.0f;
		_kind.desc.duration = 1.5f;
		_kind.desc.maxInstances = 3;
		_kind.replayInterval = 0.6f;
		_kind.offset = { -6.0f, 0.0f, 0.0f };
		_kind.step = { 0.9f, 0.0f, 0.0f };
		_kind.stepCount = 3;
	}

	// Y軸固定・半透明。横に回り込むと正面を向くが、上から見ると板が立ったまま
	static void SetupAxisY(QuadKind& _kind)
	{
		_kind.name = "AxisY";
		_kind.blendMode = MaterialBlendMode::Blend;
		_kind.look.baseColor[0] = 1.0f; _kind.look.baseColor[1] = 0.3f; _kind.look.baseColor[2] = 0.2f; _kind.look.baseColor[3] = 0.9f;
		_kind.look.flowSpeed = 0.0f;
		_kind.desc.billboardMode = BillboardMode::AxisY;
		_kind.desc.width = 1.5f;
		_kind.desc.height = 3.0f;
		_kind.desc.duration = 2.0f;
		_kind.desc.maxInstances = 3;
		_kind.replayInterval = 0.8f;
		_kind.offset = { 0.0f, 0.0f, 0.0f };
		_kind.step = { 0.0f, 0.0f, 0.9f };
		_kind.stepCount = 3;
	}

	// 固定向き・半透明。回転で向きを決める。maxInstances を2にして、3つ目の再生が断られるのを見る
	static void SetupNone(QuadKind& _kind)
	{
		_kind.name = "None";
		_kind.blendMode = MaterialBlendMode::Blend;
		_kind.look.baseColor[0] = 0.3f; _kind.look.baseColor[1] = 1.0f; _kind.look.baseColor[2] = 0.3f; _kind.look.baseColor[3] = 0.9f;
		_kind.look.flowSpeed = 1.0f;
		_kind.desc.billboardMode = BillboardMode::None;
		_kind.desc.width = 3.0f;
		_kind.desc.height = 1.5f;
		_kind.desc.duration = 1.5f;
		_kind.desc.maxInstances = 2;
		_kind.replayInterval = 0.6f; // 1.5秒の再生に0.6秒間隔なので、3つ目は空きSlotが無い
		_kind.offset = { 6.0f, 0.0f, 0.0f };
		_kind.step = { 0.0f, 0.9f, 0.0f };
		_kind.stepCount = 3;
		_kind.rotation = Quaternion::FromAxisAngle(Vector3::Right, 0.5f);
	}

	// 小さい板を64枚。0.05秒ごとに1枚ずつ再生し、3秒かけて溶ける。Materialの複製が64個要る
	static void SetupGrid(QuadKind& _kind)
	{
		_kind.name = "Grid";
		_kind.blendMode = MaterialBlendMode::Blend;
		_kind.look.baseColor[0] = 1.0f; _kind.look.baseColor[1] = 0.9f; _kind.look.baseColor[2] = 0.3f; _kind.look.baseColor[3] = 0.9f;
		_kind.desc.billboardMode = BillboardMode::FaceCamera;
		_kind.desc.width = 0.5f;
		_kind.desc.height = 0.5f;
		_kind.desc.duration = 3.0f;
		_kind.desc.maxInstances = GRID_COUNT;
		_kind.replayInterval = 0.05f;
		_kind.offset = { -2.1f, 6.0f, 0.0f };
	}

	// テンプレートのMaterialを作って見た目を入れ、板の種類として登録する
	bool RegisterQuadKind(QuadKind& _kind)
	{
		MaterialRenderDesc renderDesc{};
		renderDesc.blendMode = _kind.blendMode;
		renderDesc.doubleSided = true;
		_kind.desc.material = Gfx::CreateMaterial(pixelShader, renderDesc);
		if (!_kind.desc.material.IsValid()) return false;
		if (!Gfx::SetMaterialParameter(_kind.desc.material, 0, _kind.look)) return false;

		_kind.desc.kind = VFXMeshKind::Quad;
		_kind.meshType = Gfx::RegisterVFXMesh(_kind.desc);
		if (!_kind.meshType.IsValid())
		{
			DEBUG_LOG_ERROR("VFXメッシュの種類の登録に失敗しました({})\n", _kind.name);
			return false;
		}
		_kind.playing.reserve(_kind.desc.maxInstances);
		_kind.replayTimer = _kind.replayInterval; // 最初のフレームで1つ目を再生開始する
		return true;
	}

	bool RegisterTrail()
	{
		MaterialRenderDesc renderDesc{};
		renderDesc.blendMode = MaterialBlendMode::Blend;
		renderDesc.doubleSided = true;
		trailMaterial = Gfx::CreateMaterial(pixelShader, renderDesc);
		if (!trailMaterial.IsValid()) return false;

		LookParameter look{};
		look.baseColor[0] = 1.0f; look.baseColor[1] = 0.9f; look.baseColor[2] = 0.3f; look.baseColor[3] = 0.8f;
		look.flowSpeed = 2.0f;
		look.dissolve = 0.0f; // 帯は頂点色のαで消えるので progress では溶かさない
		if (!Gfx::SetMaterialParameter(trailMaterial, 0, look)) return false;

		trailDesc.material = trailMaterial;
		trailDesc.kind = VFXMeshKind::Trail;
		trailDesc.duration = 1.2f;       // 点を受け付ける秒数
		trailDesc.maxInstances = 2;
		trailDesc.trailPointCount = 32;
		trailDesc.trailPointLife = 0.35f;
		trailType = Gfx::RegisterVFXMesh(trailDesc);
		if (!trailType.IsValid())
		{
			DEBUG_LOG_ERROR("VFXの帯の種類の登録に失敗しました\n");
			return false;
		}
		trails.reserve(trailDesc.maxInstances);
		trailReplayTimer = trailReplayInterval; // 最初のフレームで1本目を再生開始する
		return true;
	}

	static void ShutdownQuadKind(QuadKind& _kind)
	{
		for (VFXHandle handle : _kind.playing) Gfx::StopVFX(handle, true);
		_kind.playing.clear();
		Gfx::Unload(_kind.desc.material); // Slotごとの複製はライブラリが持ち、ライブラリの終了で解放される
	}

	// 再生中の板を更新し、終了（false）したものはハンドルを外す
	static void UpdatePlaying(QuadKind& _kind, float _deltaTime)
	{
		for (std::size_t i{ 0 }; i < _kind.playing.size();)
		{
			const VFXHandle handle{ _kind.playing[i] };
			const bool alive{ Gfx::UpdateVFX(handle, _deltaTime) };
			if (!alive)
			{
				// IsFinishedVFX は終了済みのハンドルに毎回「世代が異なります」の警告を出すので ここでは呼ばない
				_kind.playing[i] = _kind.playing.back();
				_kind.playing.pop_back();
			}
			else
			{
				++i;
			}
		}
	}

	void UpdateQuadKind(QuadKind& _kind, float _deltaTime)
	{
		// 1. 一定間隔で再生開始。空きSlotが無ければ無効ハンドルが返る
		_kind.replayTimer += _deltaTime;
		if (_kind.replayTimer >= _kind.replayInterval)
		{
			_kind.replayTimer = 0.0f;
			const float stepIndex{ static_cast<float>((_kind.playCount + _kind.rejectedCount) % _kind.stepCount) };
			const Vector3 position{ center + _kind.offset + _kind.step * stepIndex };
			const VFXHandle handle{ Gfx::PlayVFX(_kind.meshType, position, _kind.rotation) };
			if (handle.IsValid())
			{
				_kind.playing.push_back(handle);
				_kind.playCount++;
			}
			else
			{
				_kind.rejectedCount++;
				DEBUG_LOG("VFXメッシュの再生開始を断られました({}) 累計{}回\n", _kind.name, _kind.rejectedCount);
			}
		}

		// 2. 再生中のものを更新
		UpdatePlaying(_kind, _deltaTime);
	}

	// 格子：0.05秒ごとに次の番号の位置へ1枚再生する。時間がずれているので順番に溶けていく
	void UpdateGrid(float _deltaTime)
	{
		grid.replayTimer += _deltaTime;
		if (grid.replayTimer >= grid.replayInterval)
		{
			grid.replayTimer = 0.0f;
			const float column{ static_cast<float>(gridNext % GRID_SIDE) };
			const float row{ static_cast<float>(gridNext / GRID_SIDE) };
			const Vector3 position{ center + grid.offset + Vector3{ column * 0.6f, row * 0.6f, 0.0f } };
			const VFXHandle handle{ Gfx::PlayVFX(grid.meshType, position, Quaternion::Identity) };
			if (handle.IsValid())
			{
				grid.playing.push_back(handle);
				grid.playCount++;
			}
			else
			{
				grid.rejectedCount++;
			}
			gridNext = (gridNext + 1) % GRID_COUNT;
		}

		UpdatePlaying(grid, _deltaTime);
	}

	void UpdateTrails(float _deltaTime)
	{
		// 1. 一定間隔で再生開始。2回に1回は受け付けの途中で StopVFX する
		trailReplayTimer += _deltaTime;
		if (trailReplayTimer >= trailReplayInterval)
		{
			trailReplayTimer = 0.0f;
			const VFXHandle handle{ Gfx::PlayVFX(trailType, center, Quaternion::Identity) };
			if (handle.IsValid())
			{
				TrailPlaying trail{};
				trail.handle = handle;
				trail.stopAtHalf = (trailPlayCount % 2) == 1;
				trails.push_back(trail);
				trailPlayCount++;
				DEBUG_LOG("VFXの帯を再生開始 累計{}回 途中で止める={}\n", trailPlayCount, trail.stopAtHalf);
			}
			else
			{
				DEBUG_LOG("VFXの帯の再生開始を断られました\n");
			}
		}

		// 2. 再生中の帯へ剣の根元と剣先を渡して更新。終了（false）したものは外す
		for (std::size_t i{ 0 }; i < trails.size();)
		{
			TrailPlaying& trail{ trails[i] };
			trail.age += _deltaTime;
			trail.angle += sweepSpeed * _deltaTime;

			// 受け付けの半分で止める。帯は残りの点が寿命で消えるまで描かれ続ける
			if (trail.stopAtHalf && !trail.stopRequested && trail.age >= trailDesc.duration * 0.5f)
			{
				Gfx::StopVFX(trail.handle, false);
				trail.stopRequested = true;
			}

			// 受け付けが終わったあとは false が返る（点は足されない）ので、戻り値は見ない
			const Vector3 direction{ std::cos(trail.angle), 0.0f, std::sin(trail.angle) };
			const Vector3 base{ center + Vector3{ 0.0f, 3.0f, 0.0f } };
			const Vector3 root{ base + direction * sweepRadiusRoot };
			const Vector3 tip{ base + direction * sweepRadiusTip + Vector3{ 0.0f, std::sin(trail.angle * 2.0f) * 0.5f, 0.0f } };
			Gfx::SetVFXTrailPoint(trail.handle, root, tip);

			const bool alive{ Gfx::UpdateVFX(trail.handle, _deltaTime) };
			if (!alive)
			{
				DEBUG_LOG("VFXの帯が終了 再生開始から{}秒\n", trail.age);
				trails[i] = trails.back();
				trails.pop_back();
			}
			else
			{
				++i;
			}
		}
	}
};
