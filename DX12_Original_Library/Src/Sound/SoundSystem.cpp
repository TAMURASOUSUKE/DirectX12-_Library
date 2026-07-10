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
				DEBUG_LOG("BGMを途中から再開しました\n");
			}
			return;
		}
		DestroySoundPair(prevBGM); // 前回切り替えたBGMがまだ残っていた場合は破棄

		// 即時停止(仮)。クロスフェード実装時にStopを外す
		currentBGM.voiceResource->Stop(0);

		prevBGM = currentBGM;
		currentBGM = {}; // 空にする
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
		voice->DestroyVoice();
		return;
	}
}

void SoundSystem::StopBGM()
{
	if (!currentBGM.voiceResource) return; // 現在再生されている物がないなら返す
	currentBGM.voiceResource->Stop(0); // その場で一時停止
	currentBGM.isPaused = true; // 停止フラグを立てる
	DEBUG_LOG("BGMを一時停止しました\n");
}

void SoundSystem::EndBGM()
{
	DestroySoundPair(currentBGM);
	DestroySoundPair(prevBGM);
	DEBUG_LOG("BGMが破棄されました\n");
}

void SoundSystem::SetVolume(SoundHandle _handle, float _valume)
{
	const float volume{ ClampVolume(_valume) }; // 0-1に単位化

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
	// 一旦切り替え前BGMを即破棄
	DestroySoundPair(prevBGM);

}

// 全てのSEが再生終了しているか
bool SoundSystem::IsStopAllSE()
{
	bool result{ true };
	for (const SoundPair& sound : liveVoices)
	{
		if (sound.voiceResource)
		{
			// 一つでも再生中のものがあればfalseになる
			if (!sound.isPaused) result = false;
		}
	}
	return result;
}

void SoundSystem:: Cleanup()
{
	// voiceをクリアする
	for (SoundPair sound : liveVoices)
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