#include "../Src/Sound/SoundResourceManager.h"

// 音関連のテスト
int main()
{
	SoundResourceManager::Instance().Initialize(); // reserve
	SoundHandle h{ SoundResourceManager::Instance().LoadSound("Test.wav") };
	return h.IsValid() ? 0 : 1; // 成否を終了コードで
}