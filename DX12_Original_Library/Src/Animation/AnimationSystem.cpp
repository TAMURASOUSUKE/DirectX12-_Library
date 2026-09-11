#include <utility>
#include <cmath>
#include <algorithm>
#include "../Debug/DebugLogs.h"
#include "../Core/Handle/HandlePacking.h"
#include "../Graphics/GraphicsResourceManager.h" // 今後リファクタリングで分離候補
#include "../Graphics/GraphicsConstant.h"
#include "AnimationSystem.h"

void AnimationSystem::Setup()
{
	Shutdown(); // 再初期化対策
	slots.reserve(MAX_ANIM_INSTANCE_COUNT); // 最大数をあらかじめ作って再確保を避ける
	
	// 姿勢計算中の再確保防止
	localPoseCache.reserve(MAX_BONE_NUM);
	sourcePoseCache.reserve(MAX_BONE_NUM);
	destinationPoseCache.reserve(MAX_BONE_NUM);
	blendedPoseCache.reserve(MAX_BONE_NUM);
}

void AnimationSystem::Shutdown()
{
	slots.clear(); // 内部で持っているAnimInstanceDataのvectorも破棄
	while (!freeList.empty())
	{
		freeList.pop();
	}

	localPoseCache.clear();
	sourcePoseCache.clear();
	destinationPoseCache.clear();
	blendedPoseCache.clear();
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

	// 初期姿勢を計算する
	UpdateGlobalPose(instance);

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

	// 通常再生中は進行中のクロスフェードを解除する
	instance->blend = AnimationBlendState{};

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
	UpdateGlobalPose(*instance);
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
	
	// 停止中はクロスフェードも終了する
	instance->blend = AnimationBlendState{};

	// 指定姿勢へ
	UpdateGlobalPose(*instance);
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

	// 遷移先が先に終了してもブレンド中ならブレンド時間は進める
	if (instance->isFinished && !instance->blend.isActive) return true; 
	if (!instance->isPlaying && !instance->isFinished) return true;
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
		UpdateGlobalPose(*instance);
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

	// ブレンドが有効な場合
	if (instance->blend.isActive)
	{
		AnimationBlendState& blend{ instance->blend };
		if (blend.sourceAnim < 0 || static_cast<std::size_t>(blend.sourceAnim) >= model->animations.size())
		{
			DEBUG_LOG_ERROR("遷移元のアニメーションクリップ番号が範囲外です\n");
			blend.isActive = false;
			return false;
		}
		const float sourceRangeDuration{ blend.sourceEndTime - blend.sourceStartTime };
		if (!std::isfinite(sourceRangeDuration) || sourceRangeDuration <= 0.0f)
		{
			DEBUG_LOG_ERROR("遷移元の再生区間が不正です\n");
			blend.isActive = false;
			return false;
		}
		blend.sourceTime += _deltaTime * blend.sourcePlaybackSpeed;
		// ループ処理
		if (blend.sourceIsLoop)
		{
			float offset{ std::fmod(blend.sourceTime - blend.sourceStartTime, sourceRangeDuration) };
			if (offset < 0.0f) offset += sourceRangeDuration;
			blend.sourceTime = blend.sourceStartTime + offset;
		}
		else if (blend.sourcePlaybackSpeed > 0.0f && blend.sourceTime >= blend.sourceEndTime)
		{
			// 通常再生
			blend.sourceTime = blend.sourceEndTime;
		}
		else if (blend.sourcePlaybackSpeed < 0.0f && blend.sourceTime <= blend.sourceStartTime)
		{
			// 逆再生
			blend.sourceTime = blend.sourceStartTime;
		}

		blend.elapsedTime += _deltaTime;
		if (blend.elapsedTime >= blend.duration)
		{
			blend.elapsedTime = blend.duration;
			blend.isActive = false;
		}
	}

	// 更新済みの時刻からボーン姿勢を再計算する
	UpdateGlobalPose(*instance);
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

bool  AnimationSystem::CrossFade(AnimInstanceHandle _handle, int _clipIndex, float _duration, bool _isLoop, float _playbackSpeed)
{
	AnimInstanceData* instance{ Lookup(_handle) };
	if (!instance)
	{
		DEBUG_LOG_ERROR("LookUpに失敗しました\n");
		return false;
	}
	ModelData* model{ GraphicsResourceManager::Instance().Lookup(instance->modelHandle) };
	if (!model)
	{
		DEBUG_LOG_ERROR("不正なモデルが渡されています\n");
		return false;
	}
	if (instance->currentAnim < 0 || static_cast<std::size_t>(instance->currentAnim) >= model->animations.size())
	{
		DEBUG_LOG_ERROR("再生中のアニメーションクリップ番号が範囲外です\n");
		instance->isPlaying = false;
		return false;
	}
	if (_clipIndex < 0 || static_cast<std::size_t>(_clipIndex) >= model->animations.size())
	{
		DEBUG_LOG_ERROR("遷移先のアニメーションクリップ番号が範囲外です\n");
		return false;
	}
	if (!std::isfinite(_duration) || _duration <= 0.0f || !std::isfinite(_playbackSpeed) || _playbackSpeed == 0.0)
	{
		DEBUG_LOG_ERROR("アニメーションの設定値に不正な値が渡されています\n");
		return false;
	}
	if (instance->blend.isActive)
	{
		DEBUG_LOG_ERROR("すでにクロスフェード中です\n");
		return false;
	}

	const Animation& destination{ model->animations[_clipIndex] };
	if (!std::isfinite(destination.duration) || destination.duration <= 0.0f)
	{
		DEBUG_LOG_ERROR("遷移先クリップの長さが不正です\n");
		return false;
	}

	// CrossFadeのまでにPlayまたはPlayRangeが行われたかチェック
	const Animation& source{ model->animations[instance->currentAnim] };
	const bool hasValidSourceRange{
	std::isfinite(instance->playbackStartTime) && // スタート時が有効値か
	std::isfinite(instance->playbackEndTime) && // スタート時が有効値か
	instance->playbackStartTime >= 0.0f && // スタート時が0以上か
	instance->playbackStartTime <
		instance->playbackEndTime && // StartよりEndの方が大きいか
	instance->playbackEndTime <= source.duration // 再生終了時刻はアニメーションの長さの範囲内か
	};

	if (!hasValidSourceRange)
	{
		DEBUG_LOG_ERROR("CrossFadeの前にPlayまたはPlayRangeを実行してください\n");
		return false;
	}

	// ブレンド設定をローカルに
	AnimationBlendState& blend{ instance->blend };
	blend.sourceAnim = instance->currentAnim;
	blend.sourceTime = instance->currentTime;
	blend.sourceStartTime = instance->playbackStartTime;
	blend.sourceEndTime = instance->playbackEndTime;
	blend.sourceIsLoop = instance->isLoop;

	blend.elapsedTime = 0.0f;
	blend.duration = _duration;
	blend.isActive = true;

	blend.sourcePlaybackSpeed = instance->playbackSpeed;
	instance->currentAnim = _clipIndex;
	instance->playbackStartTime = 0.0f;
	instance->playbackEndTime = destination.duration;
	instance->playbackSpeed = _playbackSpeed;
	instance->currentTime = _playbackSpeed > 0.0f ? 0.0f : destination.duration;

	instance->isPaused = false;
	instance->isLoop = _isLoop;
	instance->isPlaying = true;
	instance->isFinished = false;

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

void AnimationSystem::UpdateGlobalPose(AnimInstanceData& _instance)
{
	ModelData* model{ GraphicsResourceManager::Instance().Lookup(_instance.modelHandle) }; // データ部分を分解する
	if (!model) return;

	// 初回もしくはサイズが違ったときに確保しなおす
	if (_instance.globalPoses.size() != model->bones.size())
	{
		_instance.globalPoses.resize(model->bones.size()); // globalPoseのサイズ確保
		_instance.skinningMatrices.resize(model->bones.size()); // スキニング行列のサイズ確保
	}

	// アニメーションが存在し、指定番号が配列の範囲内か確認する
	const bool hasValidAnimation{ !model->animations.empty() && _instance.currentAnim >= 0 && static_cast<size_t>(_instance.currentAnim) < model->animations.size() };
	// アニメーションが存在するモデルのなのに範囲外を指定した場合は警告を出す
	if (!hasValidAnimation && !model->animations.empty()) DEBUG_LOG_WARNING("指定されたアニメーションが範囲外です。バインドポーズで描画します");

	// 補間したlocalposeを入れる一次領域を使いまわす(reserveをしているため必要な容量があればメモリの再確保が起きない)
	localPoseCache.resize(model->bones.size());
	if (hasValidAnimation)
	{
		const Animation& destination{ model->animations[_instance.currentAnim] }; // 指定の遷移先アニメーションを取り出す
		SampleAnimationTRS(destination, model->bones, _instance.currentTime, destinationPoseCache);
		const std::vector<BoneLocalPose>* finalPose{ &destinationPoseCache }; // 最終的なポーズ

		AnimationBlendState& blend{ _instance.blend };
		const bool hasValidSource{ blend.sourceAnim >= 0 && static_cast<std::size_t>(blend.sourceAnim) < model->animations.size() }; // 遷移元が範囲内か
		if (blend.isActive && !hasValidSource)
		{
			DEBUG_LOG_ERROR("クロスフェードの遷移元が無効です\n");
			blend.isActive = false;
		}
		else if (blend.isActive)
		{
			const Animation& source{ model->animations[blend.sourceAnim] };
			SampleAnimationTRS(source, model->bones, blend.sourceTime, sourcePoseCache);
			const float t{ std::clamp(blend.elapsedTime / blend.duration, 0.0f, 1.0f) }; // 混ぜ具合

			blendedPoseCache.resize(model->bones.size());
			for (std::size_t i = 0; i < model->bones.size(); i++)
			{
				const BoneLocalPose& sourcePose{ sourcePoseCache[i] };
				const BoneLocalPose& destinationPose{ destinationPoseCache[i] };
				BoneLocalPose& blendedPose{ blendedPoseCache[i] };

				// 各ポーズを補間
				blendedPose.translation = Vector3::Lerp(sourcePose.translation, destinationPose.translation, t);
				blendedPose.rotation = Quaternion::Slerp(sourcePose.rotation, destinationPose.rotation, t);
				blendedPose.scale = Vector3::Lerp(sourcePose.scale, destinationPose.scale, t);
			}
			finalPose = &blendedPoseCache;
		}
		for (std::size_t i = 0; i < model->bones.size(); i++)
		{
			const BoneLocalPose& pose{ (*finalPose)[i] };
			const Mat4x4 translation{ Mat4x4::MakeTranslation(pose.translation) };
			const Mat4x4 rotation{ pose.rotation.ToMat4x4() };
			const Mat4x4 scale{ Mat4x4::MakeScaling(pose.scale) };
			localPoseCache[i] = scale * rotation * translation;
		}
	}

	// ボーン数分回してglobal行列を求める
	for (size_t i = 0; i < model->bones.size(); i++)
	{
		const Bone& bone{ model->bones[i] };  // ボーンを取り出す 
		// アニメーションがあれば更新されたボーンのローカルポーズ、そうでなければバインドポーズ
		Mat4x4 local{ hasValidAnimation ? localPoseCache[i] : model->bones[i].localPose };

		if (bone.parentIndex < 0)
		{
			// Rootはローカルポーズがそのままグローバル行列になる(Armature変換をおこなう)
			_instance.globalPoses[i] = local * model->skeletonRoot;
		}
		else
		{
			// 自身のローカルと親との乗算を行うことで自身のglobalposeを求めることができる(親が先計算されていることが前提)
			_instance.globalPoses[i] = local * _instance.globalPoses[bone.parentIndex];
		}
	}

	// スキニング行列の計算
	for (size_t i = 0; i < model->bones.size(); i++)
	{
		// 行優先のためIBM * Globalにする
		_instance.skinningMatrices[i] = model->bones[i].inverseBindMatrix * _instance.globalPoses[i];
	}
}

void AnimationSystem::SampleAnimationTRS(const Animation& _animation, const std::vector<Bone>& _bones, float _time, std::vector<BoneLocalPose>& _outPoses)
{
	size_t boneCount{ _bones.size() }; // ボーン数
	_outPoses.resize(boneCount);

	// バインドポーズのローカルポーズからTRSを取り出して初期化
	for (size_t i = 0; i < boneCount; i++)
	{
		_outPoses[i].translation = _bones[i].bindTranslation;
		_outPoses[i].rotation = _bones[i].bindRotation;
		_outPoses[i].scale = _bones[i].bindScale;
	}

	// 全チャンネルを回してアニメーションされるボーンを上書きする
	for (const AnimChannel& ch : _animation.channels)
	{
		if (ch.times.empty() || ch.times.size() != ch.values.size())
		{
			DEBUG_LOG_WARNING("アニメーションチャンネルのキーフレームデータが不正です TimeCount : {} ValueCount : {}\n", ch.times.size(), ch.values.size());
			// このチャンネルは適用せず、バインド姿勢を維持する
			continue;
		}

		Vector4 v{ SampleChannel(ch, _time) }; // このチャンネルの補間値

		const int bone{ ch.boneIndex };
		if (bone < 0 || static_cast<std::size_t>(bone) >= boneCount)
		{
			DEBUG_LOG_WARNING("アニメーションチャンネルのBoneIndexが範囲外です BoneIndex : {} BoneCount : {}\n", bone, boneCount);
			continue;
		}

		switch (ch.path)
		{
		case AnimPath::Translation: _outPoses[bone].translation = Vector3{ v.x, v.y, v.z }; break;
		case AnimPath::Rotation: _outPoses[bone].rotation = Quaternion{ v }; break;
		case AnimPath::Scale: _outPoses[bone].scale = Vector3{ v.x, v.y, v.z }; break;
		}
	}
}

Vector4 AnimationSystem::SampleChannel(const AnimChannel& _channel, float _time)
{
	if (_channel.times.empty()) { return Vector4{}; } // キーフレームが0個の場合
	if (_channel.times.size() == 1) { return _channel.values[0]; } // キーフレームが1つなら補完せずにそのまま返す

	// 最初のキーフレームより前の位置の境界
	if (_time <= _channel.times.front()) { return _channel.values.front(); } // 補完せずに最初の要素を返す
	// 最後のキーフレームより後の位置の境界
	if (_time >= _channel.times.back()) { return _channel.values.back(); } // 補完せずに最後の要素を返す

	// 補完する二点間を探索する
	auto it{ std::upper_bound(_channel.times.begin(), _channel.times.end(), _time) }; // 二分探索を行い入力された時間の次に大きい要素のイテレータを取得する(O(logN))
	int index1{ static_cast<int>(it - _channel.times.begin()) }; // 後の要素のインデックス
	int index0{ index1 - 1 }; // 前の要素のインデックス

	// 補完を行う(回転はQuaternionで対応する)
	// 今の場所 / 全体で0-1の補完率を求める
	float ratio{ (_time - _channel.times[index0]) / (_channel.times[index1] - _channel.times[index0]) }; // 補完率
	if (_channel.path == AnimPath::Rotation)
	{
		// 回転であればSlerpで補完する
		Quaternion q0{ _channel.values[index0] }; // 前の値の四元数
		Quaternion q1{ _channel.values[index1] }; // 後の値の四元数
		Quaternion result{ Quaternion::Slerp(q0, q1, ratio) };  // 球面線形補完を行う
		return Vector4{ result.x, result.y, result.z, result.w };
	}
	else
	{
		// 通常の補完
		Vector4 v0{ _channel.values[index0] }; // 前の値
		Vector4 v1{ _channel.values[index1] }; // 後の値
		return v0 + (v1 - v0) * ratio; // 開始地点 + 全体 * 補完率でどのくらい進んだかを求める
	}
}
