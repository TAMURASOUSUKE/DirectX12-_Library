#pragma once

class GraphicsResourceManager;
class SoundResourceManager;
// ResourceManagerだけが生成できるアクセス用の鍵を作る
class PassKey
{
private:
	PassKey() = default;

	friend class GraphicsResourceManager;
	friend class SoundResourceManager;
};
