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
	bool Play(AnimInstanceHandle _handle, int _clipIndex, bool _isLoop);

	// 選択したアニメーションを指定した区間で再生する
	bool PlayRange(AnimInstanceHandle _handle, int _clipIndex, float _startTime, float _endTime, bool _isLoop);

	// 選択したアニメーションの再生を停止する
	bool Stop(AnimInstanceHandle _handle);

	// 再生時刻を進めて現在の姿勢を更新する
	bool Update(AnimInstanceHandle _handle, float _deltaTime);

	// 非ループアニメーションが終了したか
	bool IsFinished(AnimInstanceHandle _handle);

	// 世代付きハンドルから個体データを取得する
	AnimInstanceData* Lookup(AnimInstanceHandle _handle);

	// アニメーション個体を破棄して席を返却する
	void Destroy(AnimInstanceHandle _handle);

private:
	std::vector<AnimInstanceSlot> slots{}; // アニメーション個体の台帳
	std::stack<int> freeList{};            // 解放済みスロット番号
};
