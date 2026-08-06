#include <utility>
#include <cmath>
#include "../Debug/DebugLogs.h"
#include "../Core/Handle/HandlePacking.h"
#include "../Graphics/GraphicsResourceManager.h" // 今後リファクタリングで分離候補
#include "../Graphics/GraphicsConstant.h"
#include "AnimationSystem.h"

void AnimationSystem::Setup()
{
	Shutdown(); // 再初期化対策
	slots.reserve(MAX_ANIM_INSTANCE_COUNT); // 最大数をあらかじめ作って再確保を避ける
}

void AnimationSystem::Shutdown()
{
	slots.clear(); // 内部で持っているAnimInstanceDataのvectorも破棄
	while (!freeList.empty())
	{
		freeList.pop();
	}
}

AnimInstanceHandle AnimationSystem::Create(ModelHandle _modelHandle)
{
	ModelData* model{ GraphicsResourceManager::Instance().Lookup(_modelHandle) };
	if (!model)
	{
		DEBUG_LOG_ERROR("アニメーション個体の作成に無効なModelHandleが渡されました\n");
		return AnimInstanceHandle{};
	}
	// 静的モデルはボーンを持たないので弾く
	if (model->bones.empty())
	{
		DEBUG_LOG_ERROR("ボーンを持たないモデルからアニメーション個体は作成できません\n");
		return AnimInstanceHandle{};
	}
	// クリップ再生用のシステムなのでアニメーションを持たないモデルは対象外
	if (model->animations.empty())
	{
		DEBUG_LOG_ERROR("アニメーションを持たないモデルが渡されました\n");
		return AnimInstanceHandle{};
	}
	// スキニング行列の最大数チェック
	if (model->bones.size() > MAX_BONE_NUM)
	{
		DEBUG_LOG_ERROR("モデルのボーン数が上限を超えています BoneCount : {} Max : {}", model->bones.size(), MAX_BONE_NUM);
		return AnimInstanceHandle{};
	}
	// 新しく確保できるかもしくは再利用できるか
	const bool canRegister{ !freeList.empty() || slots.size() < MAX_ANIM_INSTANCE_COUNT };
	DEBUG_ASSERT(canRegister && "アニメーション個体の登録上限に達しました\n"); // 致命的なエラーなのでデバッグ時に止める
	if (!canRegister) return AnimInstanceHandle{};

	AnimInstanceData instance{};
	instance.modelHandle = _modelHandle; // 指定されたモデルハンドルを登録
	// 個体作成後、最初の描画までに行列領域が存在する用に確保する
	instance.globalPoses.resize(model->bones.size(), Mat4x4::Identity);
	instance.skinningMatrices.resize(model->bones.size(), Mat4x4::Identity);

	// 後にこの計算処理を当Systemへ移植し初期姿勢を計算する
	GraphicsResourceManager::Instance().UpdateGlobalPose(instance);

	int index{ 0 };
	if (!freeList.empty())
	{
		index = freeList.top(); // Destroyされたところから持ってくる
		freeList.pop();
		slots[index].data = std::move(instance);
	}
	else
	{
		index = static_cast<int>(slots.size()); // 空きがないなら新しく作る
		slots.push_back({ std::move(instance), 0}); // 新しく確保したので世代は0
	}

	const int packed{ Pack(index, static_cast<int>(slots[index].generation)) };
	return AnimInstanceHandle{ PassKey{}, packed };
}

bool AnimationSystem::Play(AnimInstanceHandle _handle, int _clipIndex, bool _isLoop, float _playbackSpeed)
{
	AnimInstanceData* instance{ Lookup(_handle) };
	if (!instance)
	{
		DEBUG_LOG_ERROR("アニメーション再生に無効なハンドルが渡されました\n");
		return false;
	}
	ModelData* model{ GraphicsResourceManager::Instance().Lookup(instance->modelHandle) };
	if (!model)
	{
		DEBUG_LOG_ERROR("アニメーション個体が参照するモデルは無効です\n");
		return false;
	}
	if (_clipIndex < 0 || static_cast<std::size_t>(_clipIndex) >= model->animations.size())
	{
		DEBUG_LOG_ERROR("アニメーションクリップ番号が範囲外です ClipIndex : {} ClipCount : {}", _clipIndex, model->animations.size());
		return false;
	}
	if (!std::isfinite(_playbackSpeed) || _playbackSpeed == 0.0f)
	{
		DEBUG_LOG_ERROR("再生速度には0以外の有限値を渡してください PlaybackSpeed : {}\n", _playbackSpeed);
		return false;
	}
	// 最初から最後まで再生
	return PlayRange(_handle, _clipIndex, 0.0f, model->animations[_clipIndex].duration, _isLoop, _playbackSpeed);
}

bool AnimationSystem::PlayRange(AnimInstanceHandle _handle, int _clipIndex, float _startTime, float _endTime, bool _isLoop, float _playbackSpeed)
{
	AnimInstanceData* instance{ Lookup(_handle) };
	if (!instance)
	{
		DEBUG_LOG_ERROR("アニメーション再生に無効なハンドルが渡されました\n");
		return false;
	}
	ModelData* model{ GraphicsResourceManager::Instance().Lookup(instance->modelHandle) };
	if (!model)
	{
		DEBUG_LOG_ERROR("アニメーション個体が参照するモデルは無効です\n");
		return false;
	}
	if (_clipIndex < 0 || static_cast<std::size_t>(_clipIndex) >= model->animations.size())
	{
		DEBUG_LOG_ERROR("アニメーションクリップ番号が範囲外です ClipIndex : {} ClipCount : {}", _clipIndex, model->animations.size());
		return false;
	}

	const Animation& animation{ model->animations[_clipIndex] };
	// このフラグで有効値かを見る
	const  bool isValidRange{ std::isfinite(_startTime) && std::isfinite(_endTime) && _startTime >= 0.0f && _startTime < _endTime && _endTime <= animation.duration };

	if (!isValidRange)
	{
		DEBUG_LOG_ERROR("アニメーション再生区間が不正です Start : {} End : {} Duration : {}\n", _startTime, _endTime, animation.duration);
		return false;
	}
	if (!std::isfinite(_playbackSpeed) || _playbackSpeed == 0.0f)
	{
		DEBUG_LOG_ERROR("再生速度には0以外の有限値を渡してください PlaybackSpeed : {}\n", _playbackSpeed);
		return false;
	}

	// 指定区間を入れる
	instance->currentAnim = _clipIndex;
	instance->playbackSpeed = _playbackSpeed;
	instance->playbackStartTime = _startTime;
	instance->playbackEndTime = _endTime;
	instance->currentTime = _playbackSpeed > 0 ? _startTime : _endTime; // 逆再生時は区間末尾から始める
	instance->isPaused = false;
	instance->isLoop = _isLoop;
	instance->isPlaying = true;
	instance->isFinished = false;

	// 指定姿勢へ
	GraphicsResourceManager::Instance().UpdateGlobalPose(*instance);
	return true;
}

bool AnimationSystem::ResumePlay(AnimInstanceHandle _handle)
{
	AnimInstanceData* instance{ Lookup(_handle) };
	if (!instance)
	{
		DEBUG_LOG_ERROR("アニメーション再生に無効なハンドルが渡されました\n");
		return false;
	}
	if (!instance->isPaused)
	{
		DEBUG_LOG_ERROR("一時停止中ではないアニメーションは再開できません\n");
		return false;
	}
	instance->isPaused = false;
	instance->isPlaying = true;
	instance->isFinished = false;
	return true;
}

bool AnimationSystem::Stop(AnimInstanceHandle _handle)
{
	AnimInstanceData* instance{ Lookup(_handle) };
	if (!instance)
	{
		DEBUG_LOG_ERROR("アニメーション再生に無効なハンドルが渡されました\n");
		return false;
	}
	
	instance->isPlaying = false;
	instance->isFinished = false;
	instance->isPaused = false;
	// 逆再生かどうかによって戻す位置を決める
	instance->currentTime = instance->playbackSpeed > 0.0f ? instance->playbackStartTime : instance->playbackEndTime;
	// 指定姿勢へ
	GraphicsResourceManager::Instance().UpdateGlobalPose(*instance);
	return true;
}

bool AnimationSystem::Pause(AnimInstanceHandle _handle)
{
	AnimInstanceData* instance{ Lookup(_handle) };
	if (!instance)
	{
		DEBUG_LOG_ERROR("アニメーション再生に無効なハンドルが渡されました\n");
		return false;
	}
	if (!instance->isPlaying)
	{
		DEBUG_LOG_ERROR("再生中でないアニメーションは一時停止できません\n");
		return false;
	}
	instance->isPaused = true;
	instance->isPlaying = false;
	return true;
}

bool AnimationSystem::Update(AnimInstanceHandle _handle, float _deltaTime)
{
	AnimInstanceData* instance{ Lookup(_handle) };
	if (!instance)
	{
		DEBUG_LOG_ERROR("アニメーションに無効なハンドルが渡されました\n");
		return false;
	}
	// nan,無限大,負数は受け入れない
	if (!std::isfinite(_deltaTime) || _deltaTime < 0.0f)
	{
		DEBUG_LOG_ERROR("DeltaTimeには0以上の有限値を指定してください DeltaTime : {}\n", _deltaTime);
		return false;
	}
	if (!instance->isPlaying || instance->isFinished) return true; // 停止中若しくは非ループ終了後は姿勢を維持する
	if (_deltaTime == 0.0f) return true; // 時間が進まないなら計算もしない

	ModelData* model{ GraphicsResourceManager::Instance().Lookup(instance->modelHandle) };
	if (!model)
	{
		DEBUG_LOG_ERROR("アニメーション個体が参照するモデルが無効です\n");
		instance->isPlaying = false;
		return false;
	}
	if (instance->currentAnim < 0 || static_cast<std::size_t>(instance->currentAnim) >= model->animations.size())
	{
		DEBUG_LOG_ERROR("再生中のアニメーションクリップ番号が範囲外です\n");
		instance->isPlaying = false;
		return false;
	}

	const Animation& animation{ model->animations[instance->currentAnim] }; // 指定アニメーションを取り出す

	// 長さが0では計算をしない
	if (animation.duration <= 0.0f)
	{
		DEBUG_LOG_ERROR("再生中のアニメーションクリップの長さが0以下です\n");
		instance->currentTime = 0.0f;
		instance->isPlaying = false;
		instance->isFinished = true;
		GraphicsResourceManager::Instance().UpdateGlobalPose(*instance);
		return true;
	}

	const float rangeDuration{ instance->playbackEndTime - instance->playbackStartTime };
	if (rangeDuration <= 0.0f)
	{
		DEBUG_LOG_ERROR("保存されているアニメーション区間が不正です\n");
		instance->isPlaying = false;
		return false;
	}

	// 速度を考慮して加算
	 instance->currentTime += _deltaTime * instance->playbackSpeed;

	if (instance->isLoop)
	{
		float offset{ std::fmod(instance->currentTime - instance->playbackStartTime, rangeDuration) };
		// 逆再生で負のあまりになった場合区間の長さを足して0以上に戻す
		if (offset < 0.0f) offset += rangeDuration;

		instance->currentTime = instance->playbackStartTime + offset;
	}
	else if (instance->currentTime >= instance->playbackEndTime && instance->playbackSpeed > 0.0f)
	{
		// 順再生非ループは最終時間で固定される
		instance->currentTime = instance->playbackEndTime;
		instance->isPaused = false;
		instance->isPlaying = false;
		instance->isFinished = true;
	}
	else if (instance->currentTime <= instance->playbackStartTime && instance->playbackSpeed < 0.0f)
	{
		// 逆再生非ループは先頭時間で固定
		instance->currentTime = instance->playbackStartTime;
		instance->isPaused = false;
		instance->isPlaying = false;
		instance->isFinished = true;
	}

	// 更新済みの時刻からボーン姿勢を再計算する
	GraphicsResourceManager::Instance().UpdateGlobalPose(*instance);
	return true;
}

bool AnimationSystem::IsFinished(AnimInstanceHandle _handle)
{
	AnimInstanceData* instance{ Lookup(_handle) };
	if (!instance) return false;
	return instance->isFinished;
}

bool AnimationSystem::SetAnimPlaybackSpeed(AnimInstanceHandle _handle, float _playbackSpeed)
{
	AnimInstanceData* instance{ Lookup(_handle) };
	if (!instance)
	{
		DEBUG_LOG_ERROR("アニメーション再生に無効なハンドルが渡されました\n");
		return false;
	}
	if (!std::isfinite(_playbackSpeed) || _playbackSpeed == 0.0f)
	{
		DEBUG_LOG_ERROR("再生速度には0以外の有限値を渡してください PlaybackSpeed : {}\n", _playbackSpeed);
		return false;
	}
	instance->playbackSpeed = _playbackSpeed;
	return true;
}

AnimInstanceData* AnimationSystem::Lookup(AnimInstanceHandle _handle)
{
	if (!_handle.IsValid())
	{
		DEBUG_LOG_ERROR("無効なハンドルです\n");
		return nullptr; // 無効なハンドルならnull
	}

	int packed{ _handle.GetRaw(PassKey{}) }; // 生の値(内部ハンドルを取得)
	int index{ UnpackIndex(packed) }; // index部分を取り出す
	// 範囲外チェック
	if (index < 0 || index >= static_cast<int>(slots.size()))
	{
		DEBUG_LOG_ERROR("ハンドルに範囲外のサイズが渡されました\n");
		return nullptr; // 範囲チェック
	}
	AnimInstanceSlot& slot{ slots[index] }; // スロットの指定ハンドル部分を取り出す
	// 世代チェック
	if (UnpackGen(packed) != static_cast<int>(slot.generation))
	{
		DEBUG_LOG_WARNING("世代が異なります\n");
		return nullptr; // 世代チェック
	}
	return &slot.data; // 実体を返す
}

bool AnimationSystem::Destroy(AnimInstanceHandle _handle)
{
	// ハンドルの正当性をLookupで検査する
	AnimInstanceData* data{ Lookup(_handle) };
	if (!data)
	{
		DEBUG_LOG_ERROR("不正なハンドルが渡されました\n");
		return false;
	}

	const int index{ UnpackIndex(_handle.GetRaw(PassKey{}))};
	slots[index].data = AnimInstanceData{};
	slots[index].generation++; // 世代を上げて破棄前のハンドルを再利用できなくする
	freeList.push(index); // この位置を使えるようにする
	return true;
}
