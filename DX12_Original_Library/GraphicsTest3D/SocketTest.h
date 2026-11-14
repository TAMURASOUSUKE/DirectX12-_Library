#pragma once
#include <cmath>
#include "../Src/Facade/TSLib.h"

// L3(Socketの番号と行列)のテスト 対象はPlayer.glb
// 使う公開API（ライブラリ側）:
//   Gfx::GetBoneIndex(instance, name)                  名前からBone番号(無ければ-1)
//   Gfx::GetBoneMatrix(instance, index, transform)     Boneのワールド行列
//   Gfx::GetSocketIndex(instance, name)                名前からSocket番号(無ければ-1)
//   Gfx::GetSocketMatrix(instance, index, transform)   Socketのワールド行列
// 見て確かめること:
//   1. 起動時のログで Blade_Root / Blade_Tip が0以上 無い名前が-1
//   2. 黄色の線が絵の剣と重なり アニメーション中も離れない(Socketが手に追従している)
//   3. 1秒ごとのログで 剣の長さが0.890 手と根本の距離が0.158 のまま変わらない(Transformの拡縮が1のとき)
struct SocketTest
{
	// 写しのテストで測った値(拡縮1のとき)
	static constexpr float EXPECTED_BLADE_LENGTH{ 0.890f };
	static constexpr float EXPECTED_HAND_TO_ROOT{ 0.158f };
	static constexpr float TOLERANCE{ 0.005f }; // 許す誤差
	static constexpr float LOG_INTERVAL{ 1.0f }; // ログを出す間隔(秒)

	int handBone{ -1 }; // mixamorig:Hand_R
	int bladeRootSocket{ -1 }; // Blade_Root
	int bladeTipSocket{ -1 }; // Blade_Tip
	float logTimer{ 0.0f }; // 次のログまでの経過秒

	// 番号を名前から1回だけ調べる(毎フレームの文字列検索をしないため)
	bool Setup(AnimInstanceHandle _instance)
	{
		handBone = Gfx::GetBoneIndex(_instance, "mixamorig:Hand_R");
		bladeRootSocket = Gfx::GetSocketIndex(_instance, "Blade_Root");
		bladeTipSocket = Gfx::GetSocketIndex(_instance, "Blade_Tip");
		const int missingSocket{ Gfx::GetSocketIndex(_instance, "NoSuchSocket") };

		DEBUG_LOG("[SocketTest] hand={} bladeRoot={} bladeTip={} 無い名前={}\n", handBone, bladeRootSocket, bladeTipSocket, missingSocket);

		if (missingSocket != -1) DEBUG_LOG_ERROR("[SocketTest] 無い名前なのに-1以外が返りました\n");
		return handBone >= 0 && bladeRootSocket >= 0 && bladeTipSocket >= 0;
	}

	// Gfx::UpdateAnimの後に呼ぶ(そのフレームの姿勢で取るため)
	void Update(AnimInstanceHandle _instance, const Transform& _transform, float _deltaTime)
	{
		// 番号が取れていないときは行列を聞かない(毎フレームのエラーログを避ける)
		if (handBone < 0 || bladeRootSocket < 0 || bladeTipSocket < 0) return;

		// 行列の原点を移した先がそのnodeのワールド位置
		//const Vector3 handPosition{ Gfx::GetBoneMatrix(_instance, handBone, _transform).TransformPoint(Vector3::Zero) };
		//const Vector3 rootPosition{ Gfx::GetSocketMatrix(_instance, bladeRootSocket, _transform).TransformPoint(Vector3::Zero) };
		//const Vector3 tipPosition{ Gfx::GetSocketMatrix(_instance, bladeTipSocket, _transform).TransformPoint(Vector3::Zero) };
		const Vector3 handPosition{ Gfx::GetBonePosition(_instance, handBone, _transform) };
		const Vector3 rootPosition{ Gfx::GetSocketPosition(_instance, bladeRootSocket, _transform) };
		const Vector3 tipPosition{ Gfx::GetSocketPosition(_instance, bladeTipSocket, _transform) };

		// 根本から先端へ線を引く 絵の剣と重なれば位置が合っている
		Gfx::DrawLine3D(rootPosition, tipPosition, { 1.0f, 1.0f, 0.0f, 1.0f });
		// 手から根本へ線を引く
		Gfx::DrawLine3D(handPosition, rootPosition, { 0.0f, 1.0f, 1.0f, 1.0f });

		logTimer += _deltaTime;
		if (logTimer < LOG_INTERVAL) return;
		logTimer = 0.0f;

		const float bladeLength{ Vector3::Distance(rootPosition, tipPosition) };
		const float handToRoot{ Vector3::Distance(handPosition, rootPosition) };
		const bool isBladeLengthOk{ std::abs(bladeLength - EXPECTED_BLADE_LENGTH) <= TOLERANCE };
		const bool isHandToRootOk{ std::abs(handToRoot - EXPECTED_HAND_TO_ROOT) <= TOLERANCE };

		DEBUG_LOG("[SocketTest] 剣の長さ={:.3f}(期待{:.3f} {}) 手と根本={:.3f}(期待{:.3f} {}) 先端=({:.3f}, {:.3f}, {:.3f})\n",
			bladeLength, EXPECTED_BLADE_LENGTH, isBladeLengthOk ? "OK" : "NG",
			handToRoot, EXPECTED_HAND_TO_ROOT, isHandToRootOk ? "OK" : "NG",
			tipPosition.x, tipPosition.y, tipPosition.z);
	}
};
