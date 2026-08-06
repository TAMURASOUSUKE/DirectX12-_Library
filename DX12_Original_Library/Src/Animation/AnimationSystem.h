#pragma once
#include <stack>
#include <vector>
#include "../Core/Handle/AnimInstanceHandle.h"
#include "../Graphics/GraphicsType.h"

// 3Dアニメーション個体の生成・検索・破棄を管理する
class AnimationSystem
{
public:
	AnimationSystem() = default;
	~AnimationSystem() = default;

	AnimationSystem(const AnimationSystem& _other) = delete;
	AnimationSystem& operator=(const AnimationSystem& _other) = delete;

	// 台帳の初期化
	void Setup();

	// 台帳と全アニメーション個体を破棄する
	void Shutdown();

	// モデルを基にアニメーション個体を作成する
	AnimInstanceHandle Create(ModelHandle _modelHandle);

	// 選択したアニメーションを先頭から再生する
	bool Play(AnimInstanceHandle _handle, int _clipIndex, bool _isLoop, float _playbackSpeed = 1.0f);

	// 選択したアニメーションを指定した区間で再生する
	bool PlayRange(AnimInstanceHandle _handle, int _clipIndex, float _startTime, float _endTime, bool _isLoop, float _playbackSpeed = 1.0f);

	// ポーズされているアニメーションを再開する
	bool ResumePlay(AnimInstanceHandle _handle);

	// 選択したアニメーションの再生を停止して最初に戻す
	bool Stop(AnimInstanceHandle _handle);

	// 選択したアニメーションの再生を停止して維持する
	bool Pause(AnimInstanceHandle _handle);

	// 再生時刻を進めて現在の姿勢を更新する
	bool Update(AnimInstanceHandle _handle, float _deltaTime);

	// 非ループアニメーションが終了したか
	bool IsFinished(AnimInstanceHandle _handle);

	// 指定したアニメーションの再生速度を変更する(負数なら逆再生)
	bool SetAnimPlaybackSpeed(AnimInstanceHandle _handle, float _playbackSpeed);

	// 世代付きハンドルから個体データを
	AnimInstanceData* Lookup(AnimInstanceHandle _handle);

	// アニメーション個体を破棄して席を返却する
	bool Destroy(AnimInstanceHandle _handle);

private:
	std::vector<AnimInstanceSlot> slots{}; // アニメーション個体の台帳
	std::stack<int> freeList{};            // 解放済みスロット番号
};
