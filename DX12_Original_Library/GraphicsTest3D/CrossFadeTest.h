#pragma once
#include <array>
#include "../Src/Facade/TSLib.h"

// L4(クロスフェード中のクロスフェード)のテスト 対象はPlayer.glb
struct CrossFadeTest
{
	// 混ぜる時間(秒) ゲームと同じ長さ 手で割り込むときは GraphicsTest3Dの左キーでTimeScaleを0.1まで下げる(実時間で1.2秒になる)
	static constexpr float FADE_DURATION{ 0.12f };

	// キー1つに割り当てる遷移先
	struct KeyClip
	{
		KeyCode::Button key;
		int clipIndex; // Player.glbのクリップ番号
		bool isLoop;
		const char* clipName;
	};

	static constexpr std::array<KeyClip, 4> KEY_CLIPS
	{ {
		{ KeyCode::Button::J, 0, true, "00_Idle_Ground" },
		{ KeyCode::Button::K, 2, true, "02_Dash_Forward" },
		{ KeyCode::Button::L, 3, false, "03_StepF_Just" },
		{ KeyCode::Button::M, 24, false, "19_Attack_Ground_Strong_1" },
	} };

	int currentClip{ 0 }; // いま遷移先になっているクリップ番号
	int transitionCount{ 0 }; // CrossFadeAnimを呼んだ回数
	int failedCount{ 0 }; // CrossFadeAnimがfalseを返した回数
	float timeSinceLastFade{ FADE_DURATION }; // 最後に成功したCrossFadeAnimからの秒数(TimeScale適用後)
	// 最初に再生しているクリップ番号を渡す
	void Setup(int _startClip)
	{
		currentClip = _startClip;
	}

	// Gfx::UpdateAnimの前に呼ぶ
	void Update(AnimInstanceHandle _instance, float _deltaTime)
	{
		timeSinceLastFade += _deltaTime;

		for (const KeyClip& keyClip : KEY_CLIPS)
		{
			if (!Input::IsKeyPushed(keyClip.key)) continue;

			// 前のフェードが終わる前に押されたら割り込み
			const bool isInterrupting{ timeSinceLastFade < FADE_DURATION };
			const bool isSucceeded{ Gfx::CrossFadeAnim(_instance, keyClip.clipIndex, FADE_DURATION, keyClip.isLoop) };

			transitionCount++;
			if (!isSucceeded) failedCount++;

			DEBUG_LOG("[CrossFadeTest] {}回目 {} -> {}({}) 割り込み={} 前のフェードから{:.2f}秒 結果={} 失敗の累計={}\n",
				transitionCount, currentClip, keyClip.clipIndex, keyClip.clipName,
				isInterrupting, timeSinceLastFade, isSucceeded ? "成功" : "失敗", failedCount);

			if (!isSucceeded) continue;
			currentClip = keyClip.clipIndex;
			timeSinceLastFade = 0.0f;
		}
	}
};
