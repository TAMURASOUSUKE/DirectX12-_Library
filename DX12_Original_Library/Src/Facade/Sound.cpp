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

void SoundInternal::BeginFrame()
{

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

void Sound::PlaySE(SoundHandle _handle)
{
	soundSystem.PlaySE(_handle);
}