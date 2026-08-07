#pragma once

class GraphicsResourceManager;
class SoundResourceManager;
class AnimationSystem;
// 必要なものだけが生成できるアクセス用の鍵を作る
class PassKey
{
private:
	PassKey() = default;

	friend class GraphicsResourceManager;
	friend class SoundResourceManager;
	friend class AnimationSystem;
};
