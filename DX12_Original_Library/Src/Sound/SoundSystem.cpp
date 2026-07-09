#include <windows.h>
#include "../Debug/DebugLogs.h"
#include "SoundResourceManager.h"
#include "SoundSystem.h"
#pragma comment(lib, "xaudio2.lib")

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

void SoundSystem::PlaySE(SoundHandle _handle)
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
	result = voice->Start(); // 再生開始
	if (FAILED(result))
	{
		DEBUG_LOG_ERROR("SE再生に失敗しました\n");
		voice->DestroyVoice();
		return;
	}
	liveVoices.push_back(voice); // 追加
}

void SoundSystem::EndFrameCleanup()
{
	// eraseでイテレータの進め方を分岐させる
	for (auto it = liveVoices.begin(); it != liveVoices.end();)
	{
		IXAudio2SourceVoice* voice{ *it };
		XAUDIO2_VOICE_STATE state{};
		voice->GetState(&state);

		// キューに残っているバッファが0なら再生終了
		if (state.BuffersQueued == 0)
		{
			voice->DestroyVoice();
			it = liveVoices.erase(it); // 消して次のイテレータを取得
			DEBUG_LOG("再生終了した音がフレーム最後で回収されました\n");
		}
		else
		{
			++it; // 0でなければ次へ
		}

	}
}

void SoundSystem:: Cleanup()
{
	// voiceをクリアする
	for (IXAudio2SourceVoice* voice : liveVoices)
	{
		if (voice)
		{
			voice->DestroyVoice();
		}
	}
	liveVoices.clear();

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