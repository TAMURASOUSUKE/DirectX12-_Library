#pragma once
#include "../../Src/Facade/TSLib.h"
#include "ObjectBase.h"
// 背景
class Background : public ObjectBase
{
public:
	Background(TexHandle _handle, const Vector2& _position, const Vector2  _size, float _rotate) : ObjectBase(_handle, _position, _size, _rotate) {}

	void Update() override; // 更新
	void Draw() override; // 描画

	ObjectType GetObjectType() const override
	{
		return ObjectType::Background;
	}
};