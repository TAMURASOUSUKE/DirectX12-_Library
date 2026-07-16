#include <windows.h>
#include <algorithm>
#include "../Debug/DebugLogs.h"
#include "SoundResourceManager.h"
#include "SoundSystem.h"
#pragma comment(lib, "xaudio2.lib")

// ヘルパーを定義する
namespace
{
	// 音量をクランプした値を返す
	float ClampVolume(float _value)
	{
		return std::clamp(_value, 0.0f, 1.0f);
	}

	// 音量の適用
	void ApplyVolume(SoundPair& _sound, float _categoryVolume)
	{
		if (!_sound.voiceResource) return;
		// 適用(全ての音量要素を乗算する)
		_sound.voiceResource->SetVolume(_sound.volume * _sound.fadeVolume * _categoryVolume);
	}

	// 削除関数
	void DestroySoundPair(SoundPair& _sound)
	{
		if (_sound.voiceResource)
		{
			_sound.voiceResource->Stop(0); // 再生停止
			_sound.voiceResource->FlushSourceBuffers(); // キューのクリア
			_sound.voiceResource->DestroyVoice(); // 破棄
		}
		_sound = {}; // 空を入れる
	}
}

bool SoundSystem::Setup()
{
	HRESULT result{}; // 結果判定用
	result = XAudio2Create(&audioEngine, 0, XAUDIO2_DEFAULT_PROCESSOR); // エンジンを初期化する(作製する)
	DEBUG_ASSERT(SUCCEEDED(result) && "XAudioの初期化に失敗しました\n");
	if (FAILED(result)) return false;

	// スピーカーへの最終出力を作成
	result = audioEngine->CreateMasteringVoice(&masterVoice);
	DEBUG_ASSERT(SUCCEEDED(result) && "XAudioの最終出力の設定に失敗しました\n");
	if (FAILED(result)) return false;
	
	return true;
}

void SoundSystem::Update(float _deltaTime)
{
	if (!isCrossfading) return; // フェード中のフラグが立っていないと行わない
	if (currentBGM.isPaused) return; // 再生停止中なら計算も止める

	crossfadeElapsedTime += (std::max)(_deltaTime, 0.0f); // 経過時間を進める

	// 経過時間 / 総時間を0-1の範囲に収めてどのくらい進んでいるか割合にする
	const float rate{ std::clamp(crossfadeElapsedTime / crossfadeTotalTime, 0.0f, 1.0f) };

	if (prevBGM.voiceResource)
	{
		// fade中に次のfadeに移行した場合も考慮して保存していたprevFadeStartVolumeを使う
		prevBGM.fadeVolume = prevFadeStartVolume * (1.0f - rate); // prevは小さくしていくのでrateが大きくなれば乗算する値が小さくなるように1から引く
		ApplyVolume(prevBGM, bgmVolume);
	}

	if (currentBGM.voiceResource)
	{
		currentBGM.fadeVolume = rate;
		ApplyVolume(currentBGM, bgmVolume);
	}

	// フェードが完了した場合
	if (rate >= 1.0f)
	{
		DestroySoundPair(prevBGM); // フェードが完了しているので前の音は消す

		if (currentBGM.voiceResource)
		{
			currentBGM.fadeVolume = 1.0f;
			ApplyVolume(currentBGM, bgmVolume);
		}

		// リセット処理
		isCrossfading = false; // フェードを終了
		crossfadeElapsedTime = 0.0f;
		crossfadeTotalTime = 0.0f;
		prevFadeStartVolume = 1.0f;

		DEBUG_LOG("BGMのクロスフェードが完了しました\n");
	}

}

void SoundSystem::PlaySE(SoundHandle _handle, float _volume)
{
	SoundData* sd{ SoundResourceManager::Instance().Lookup(_handle) }; // ハンドル分解してデータを取り出す
	if (!sd) return;

	HRESULT result{};
	IXAudio2SourceVoice* voice{ nullptr }; // まずリソースを空で作る
	result = audioEngine->CreateSourceVoice(&voice, &sd->wavefmt); // フォーマットからボイスを作りリソースに入れる
	if (FAILED(result))
	{
		DEBUG_LOG_ERROR("ソース作成に失敗しました\n");
		return;
	}
	XAUDIO2_BUFFER buf{}; // PCMを積むバッファー
	buf.pAudioData = sd->data.data(); // 本体データを渡す
	buf.AudioBytes = static_cast<UINT32>(sd->data.size()); // データサイズ
	buf.Flags = XAUDIO2_END_OF_STREAM; // 再生終了時にBufferQueuedが0になる
	result = voice->SubmitSourceBuffer(&buf); // 実際にPCMを積む
	if (FAILED(result))
	{
		DEBUG_LOG_ERROR("ソースバッファーの投入に失敗しました\n");
		voice->DestroyVoice();
		return;
	}

	// ハンドルと実データを紐づけて保持する
	SoundPair sound{};
	sound.handle = _handle;
	sound.voiceResource = voice;
	sound.volume = ClampVolume(_volume);
	ApplyVolume(sound, seVolume); // volumeの適用

	result = voice->Start(); // 再生開始
	if (FAILED(result))
	{
		DEBUG_LOG_ERROR("SE再生に失敗しました\n");
		voice->DestroyVoice();
		return;
	}
	liveVoices.push_back(sound); // 追加
}

void SoundSystem::PlayBGM(SoundHandle _handle, bool _isLoop, float _volume)
{
	SoundData* sd{ SoundResourceManager::Instance().Lookup(_handle) }; // ハンドル分解してデータを取り出す
	if (!sd) return;

	// すでにBGMボイスが存在している場合
	if (currentBGM.voiceResource)
	{
		XAUDIO2_VOICE_STATE state{};
		currentBGM.voiceResource->GetState(&state, XAUDIO2_VOICE_NOSAMPLESPLAYED); // 大まかな計算をスキップして状態を取得する
		// キューにデータが残っているつまり再生途中かつ同じBGMならそのまま再開する
		if (state.BuffersQueued > 0 && currentBGM.handle == _handle)
		{
			if (currentBGM.isPaused)
			{
				currentBGM.voiceResource->Start(0); // 途中から再開
				currentBGM.isPaused = false; // 停止フラグを落とす

				// クロスフェード中だった場合prevも再開させる必要がある
				if (isCrossfading && prevBGM.voiceResource && prevBGM.isPaused)
				{
					prevBGM.voiceResource->Start(0); // 途中から再開
					prevBGM.isPaused = false; // 停止フラグを落とす
				}

				DEBUG_LOG("BGMを途中から再開しました\n");
			}
			return;
		}

		// すでに再生されてる物があるなら即破棄して次へ
		// 通常再生なので、進行中のクロスフェードを解除
		isCrossfading = false;
		crossfadeElapsedTime = 0.0f;
		crossfadeTotalTime = 0.0f;
		prevFadeStartVolume = 1.0f;

		DestroySoundPair(prevBGM); 
		DestroySoundPair(currentBGM);
	}

	HRESULT result{};
	IXAudio2SourceVoice* voice{ nullptr }; // まずリソースを空で作る
	result = audioEngine->CreateSourceVoice(&voice, &sd->wavefmt); // フォーマットからボイスを作りリソースに入れる
	if (FAILED(result))
	{
		DEBUG_LOG_ERROR("ソース作成に失敗しました\n");
		return;
	}
	XAUDIO2_BUFFER buf{}; // PCMを積むバッファー
	buf.pAudioData = sd->data.data(); // 本体データを渡す
	buf.AudioBytes = static_cast<UINT32>(sd->data.size()); // データサイズ
	buf.Flags = XAUDIO2_END_OF_STREAM; // 再生終了時にBufferQueuedが0になる
	if(_isLoop) buf.LoopCount = XAUDIO2_LOOP_INFINITE; // ループさせ続ける
	result = voice->SubmitSourceBuffer(&buf); // 実際にPCMを積む
	if (FAILED(result))
	{
		DEBUG_LOG_ERROR("ソースバッファーの投入に失敗しました\n");
		voice->DestroyVoice();
		return;
	}

	currentBGM.handle = _handle;
	currentBGM.voiceResource = voice;
	currentBGM.volume = ClampVolume(_volume); // 等倍
	currentBGM.fadeVolume = 1.0f; // 完全に新しいBGMに寄せる
	currentBGM.isPaused = false; // 停止フラグは立てない

	ApplyVolume(currentBGM, bgmVolume); // 音量適用

	result = voice->Start(); // 再生開始
	if (FAILED(result))
	{
		DEBUG_LOG_ERROR("BGM再生に失敗しました\n");
		DestroySoundPair(currentBGM);
		return;
	}
}

void SoundSystem::CrossfadeBGM(SoundHandle _afterBGM, bool _isLoop, float _totalFadeTime, float _volume)
{
	// 0秒以下なら即切り替え
	if (_totalFadeTime <= 0.0f)
	{
		PlayBGM(_afterBGM, _isLoop, _volume);
		return;
	}

	// 同じBGMへのクロスフェードは行わない
	if (currentBGM.voiceResource && currentBGM.handle == _afterBGM)
	{
		return;
	}

	SoundData* sd{ SoundResourceManager::Instance().Lookup(_afterBGM) };
	if (!sd) return;

	IXAudio2SourceVoice* voice{ nullptr };

	HRESULT result{ audioEngine->CreateSourceVoice(&voice, &sd->wavefmt) };
	if (FAILED(result))
	{
		DEBUG_LOG_ERROR("クロスフェード先のBGMの生成に失敗しました\n");
		return;
	}

	XAUDIO2_BUFFER buffer{};
	buffer.pAudioData = sd->data.data(); // データ本体
	buffer.AudioBytes = static_cast<UINT32>(sd->data.size()); // データのサイズ
	buffer.Flags = XAUDIO2_END_OF_STREAM;  // 再生終了時にBufferQueuedが0になる
	if (_isLoop) buffer.LoopCount = XAUDIO2_LOOP_INFINITE; // 呼び出し方によってループするか決める
	result = voice->SubmitSourceBuffer(&buffer); // キューに積む
	if (FAILED(result))
	{
		DEBUG_LOG_ERROR("クロスフェード先BGMのバッファ投入に失敗しました\n");
		voice->DestroyVoice();
		return;
	}

	// 先に現在のBGMをprevに渡すとその後新規ボイス制作に失敗したら中途半端になるためまずローカルで成功させる
	SoundPair nextBGM{};
	nextBGM.handle = _afterBGM;
	nextBGM.voiceResource = voice;
	nextBGM.volume = ClampVolume(_volume);
	nextBGM.fadeVolume = 0.0f;
	nextBGM.isPaused = false;
	ApplyVolume(nextBGM, bgmVolume);

	result = voice->Start(0);
	if (FAILED(result))
	{
		DEBUG_LOG_ERROR("クロスフェード先BGMの再生に失敗しました\n");
		DestroySoundPair(nextBGM);
		return;
	}

	// フェードアウト前のBGMをprevに入れたいのでprevに残っているBGMを掃除
	DestroySoundPair(prevBGM);

	// 現在のBGMをフェードアウト対象であるprevヘ
	if (currentBGM.voiceResource)
	{
		prevBGM = currentBGM;
		prevFadeStartVolume = prevBGM.fadeVolume; // 切り替え時のfadeボリュームを保存
		currentBGM = {};
	}
	else
	{
		prevFadeStartVolume = 0.0f; // curretがないときは0.0fで掃除
	}

	// 作成に成功したBGMをフェードイン側に登録
	currentBGM = nextBGM;

	// クロスフェード用メンバ設定
	crossfadeElapsedTime = 0.0f; // フェードがスタートするので経過時間に0を入れる
	crossfadeTotalTime = _totalFadeTime; // 指定されたフェード時間を設定
	isCrossfading = true; // フェード中のフラグを立てる
}

void SoundSystem::StopBGM()
{
	// クロスフェード用に両方止める
	
	if (currentBGM.voiceResource)
	{
		currentBGM.voiceResource->Stop(0); // その場で一時停止
		currentBGM.isPaused = true; // 停止フラグを立てる
	}

	if (prevBGM.voiceResource)
	{
		prevBGM.voiceResource->Stop(0);
		prevBGM.isPaused = true;
	}
	DEBUG_LOG("BGMを一時停止しました\n");
}

void SoundSystem::EndBGM()
{
	DestroySoundPair(currentBGM);
	DestroySoundPair(prevBGM);

	// 状態も初期化
	isCrossfading = false;
	crossfadeElapsedTime = 0.0f;
	crossfadeTotalTime = 0.0f;
	prevFadeStartVolume = 1.0f;

	DEBUG_LOG("BGMが破棄されました\n");
}

void SoundSystem::SetVolume(SoundHandle _handle, float _volume)
{
	const float volume{ ClampVolume(_volume) }; // 0-1に単位化

	// SEだった場合
	for (SoundPair& sound : liveVoices)
	{
		// ハンドルが同じものに対して音量を適用する
		if (sound.handle == _handle)
		{
			sound.volume = volume;
			ApplyVolume(sound, seVolume);
		}
	}

	// BGMだった場合
	if (currentBGM.voiceResource && currentBGM.handle == _handle)
	{
		currentBGM.volume = volume;
		ApplyVolume(currentBGM, bgmVolume);
	}

	if (prevBGM.voiceResource && prevBGM.handle == _handle)
	{
		prevBGM.volume = volume;
		ApplyVolume(prevBGM, bgmVolume);
	}
}

void SoundSystem::SetAllVolume(float _volume)
{
	if (masterVoice)
	{
		masterVoice->SetVolume(ClampVolume(_volume)); // クランプして音量制御
	}
}

void SoundSystem::SetAllSEVolume(float _volume)
{
	seVolume = ClampVolume(_volume);

	for (SoundPair& sound : liveVoices)
	{
		ApplyVolume(sound, seVolume);
	}
}

void SoundSystem::SetAllBGMVolume(float _volume)
{
	bgmVolume = ClampVolume(_volume);

	ApplyVolume(currentBGM, bgmVolume);
	ApplyVolume(prevBGM, bgmVolume);
}

void SoundSystem::EndFrameCleanup()
{
	// eraseでイテレータの進め方を分岐させる
	for (auto it = liveVoices.begin(); it != liveVoices.end();)
	{
		SoundPair& sound{ *it };
		// 音がない場合は消して次へ
		if (!sound.voiceResource)
		{
			it = liveVoices.erase(it);
			continue;
		}
		XAUDIO2_VOICE_STATE state{};
		sound.voiceResource->GetState(&state, XAUDIO2_VOICE_NOSAMPLESPLAYED);

		// キューに残っているバッファが0なら再生終了
		if (state.BuffersQueued == 0)
		{
			DestroySoundPair(sound);
			it = liveVoices.erase(it); // 消して次のイテレータを取得
			DEBUG_LOG("再生終了した音がフレーム最後で回収されました\n");
		}
		else
		{
			++it; // 0でなければ次へ
		}
	}
}

// 全てのSEが再生終了しているか
bool SoundSystem::IsStopAllSE()
{
	bool result{ true };
	for (const SoundPair& sound : liveVoices)
	{
		if (sound.voiceResource)
		{
			XAUDIO2_VOICE_STATE state{};
			sound.voiceResource->GetState(&state);
			// 一つでも再生されていればそこでループをやめてfalseを返す
			if (state.BuffersQueued > 0)
			{
				result = false; 
				break;
			}
		}
	}
	return result;
}

void SoundSystem:: Cleanup()
{
	// voiceをクリアする
	for (SoundPair& sound : liveVoices)
	{
		if (sound.voiceResource)
		{
			DestroySoundPair(sound);
		}
	}
	liveVoices.clear();

	if (currentBGM.voiceResource) DestroySoundPair(currentBGM);
	if (prevBGM.voiceResource) DestroySoundPair(prevBGM);

	 if (masterVoice)
    {
        masterVoice->DestroyVoice();
        masterVoice = nullptr;
    }

    if (audioEngine)
    {
		// デストラクタではなく明示的にここで廃棄することでCoUnInitializeするより前に破棄できる
		audioEngine.Reset();
    }
}