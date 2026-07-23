#pragma once
#include "../../Src/Facade/TSLib.h"
#include "ObjectBase.h"

class Car : public ObjectBase
{
public:
	Car(TexHandle _handle, const Vector2& _position, const Vector2  _size, float _rotate, float _speed) : ObjectBase(_handle, _position, _size, _rotate), speed{_speed} {}

	void Update() override;
	void Draw() override;

	ObjectType GetObjectType() const override
	{
		return ObjectType::Car;
	}

private:
	float speed{0.0f}; // 一秒間に進むスピード
};