#pragma once
#include "../Src/Facade/TSLib.h"

// Root Motionのテスト 対象はPlayer.glb
struct RootMotionTest
{
	static constexpr float LOG_INTERVAL{ 0.5f }; // 位置をログに出す間隔(秒)
	static constexpr float LOG_MOVE_THRESHOLD{ 0.001f }; // この距離より動いていなければログを出さない

	Vector3 startPosition{ Vector3::Zero }; // テスト開始時の位置
	Vector3 lastLoggedPosition{ Vector3::Zero }; // 最後にログへ出した位置
	float logTimer{ 0.0f }; // 次のログまでの経過秒
	bool isVertical{ false }; // 高さも取り出しているか

	// 腰のBoneをRoot Motionの取り出し元に設定する
	bool Setup(AnimInstanceHandle _instance, const Transform& _transform)
	{
		startPosition = _transform.GetPosition();
		lastLoggedPosition = startPosition;

		const int hipsBone{ Gfx::GetBoneIndex(_instance, "mixamorig:Hips") };
		const bool isSucceeded{ Gfx::SetAnimRootMotionBone(_instance, hipsBone) };
		DEBUG_LOG("[RootMotionTest] hips={} 設定={}\n", hipsBone, isSucceeded);
		return hipsBone >= 0 && isSucceeded;
	}

	// Gfx::UpdateAnimの後に呼ぶ(そのフレームで進んだ分を受け取るため)
	void Update(AnimInstanceHandle _instance, Transform& _transform, float _deltaTime)
	{
		// 高さも取り出すかの切り替え
		if (Input::IsKeyPushed(KeyCode::Button::N))
		{
			isVertical = !isVertical;
			const bool isSucceeded{ Gfx::SetAnimRootMotionVertical(_instance, isVertical) };
			DEBUG_LOG("[RootMotionTest] 高さも取り出す={} 設定={}\n", isVertical, isSucceeded);
		}

		// 位置を最初の場所へ戻す
		if (Input::IsKeyPushed(KeyCode::Button::G))
		{
			_transform.SetPosition(startPosition);
			DEBUG_LOG("[RootMotionTest] 位置を最初へ戻しました\n");
		}

		// アニメーションが進めた分の移動を受け取り 体の向きへ回してから足す(ゲーム側と同じ使い方)
		const Vector3 delta{ Gfx::ConsumeAnimRootMotion(_instance) };
		_transform.Translate(_transform.GetRotation().RotateVector(delta));

		// 動いたときだけ 一定間隔で位置を出す
		logTimer += _deltaTime;
		if (logTimer < LOG_INTERVAL) return;
		logTimer = 0.0f;

		const Vector3 position{ _transform.GetPosition() };
		if (Vector3::Distance(position, lastLoggedPosition) < LOG_MOVE_THRESHOLD) return;
		lastLoggedPosition = position;

		DEBUG_LOG("[RootMotionTest] 位置=({:.3f}, {:.3f}, {:.3f}) 最初からの距離={:.3f}\n",
			position.x, position.y, position.z, Vector3::Distance(position, startPosition));
	}
};
