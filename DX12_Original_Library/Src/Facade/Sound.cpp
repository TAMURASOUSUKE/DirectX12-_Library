#include "../Sound/SoundSystem.h"
#include "SoundInternal.h"
#include "../Sound/SoundResourceManager.h"
#include "Sound.h"

namespace
{
	SoundSystem soundSystem{};
}

bool SoundInternal::Initialize()
{
	SoundResourceManager::Instance().Initialize();
	if (!soundSystem.Setup()) return false;
	return true;
}

void SoundInternal::BeginFrame(float _deltaTime)
{
	soundSystem.Update(_deltaTime);
}

void SoundInternal::EndFrame()
{
	soundSystem.EndFrameCleanup();
}

void SoundInternal::Finish()
{
	soundSystem.Cleanup(); // 終了処理
}

SoundHandle Sound::LoadSound(const char* _filePath)
{
	return SoundResourceManager::Instance().LoadSound(_filePath);
}

void Sound::PlaySE(SoundHandle _handle, float _volume)
{
	soundSystem.PlaySE(_handle, _volume);
}

void Sound::PlayBGM(SoundHandle _handle, bool _isLoop, float _volume)
{
	soundSystem.PlayBGM(_handle, _isLoop, _volume);
}

void Sound::CrossfadeBGM(SoundHandle _afterBGM, bool _isLoop ,float _totalFadeTime, float _volume)
{
	soundSystem.CrossfadeBGM(_afterBGM, _isLoop, _totalFadeTime, _volume);
}

void Sound::StopBGM()
{
	soundSystem.StopBGM();
}

void Sound::EndBGM()
{
	soundSystem.EndBGM();
}

void Sound::SetVolume(SoundHandle _handle, float _volume)
{
	soundSystem.SetVolume(_handle, _volume);
}

void Sound::SetAllVolume(float _volume)
{
	soundSystem.SetAllVolume(_volume);
}

void Sound::SetAllBGMVolume(float _volume)
{
	soundSystem.SetAllBGMVolume(_volume);
}

void Sound::SetAllSEVolume(float _volume)
{
	soundSystem.SetAllSEVolume(_volume);
}

bool Sound::IsStopAllSE()
{
	return soundSystem.IsStopAllSE();
}