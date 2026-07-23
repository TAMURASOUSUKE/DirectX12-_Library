#pragma once
#include <functional>
#include "../../Src/Facade/TSLib.h"

// オブジェクト生成
class ObjectFactory
{
public:
	static void CreateBackground(TexHandle _handle, const Vector2& _position, const Vector2  _size, float _rotate);
	static void CreatePlayer(TexHandle _handle, const Vector2& _position, const Vector2  _size, float _rotate, float _speed, std::function<void()> _onGoal, std::function<void(int)> _onLifeChange);
	static void CreateCar(TexHandle _handle, const Vector2& _position, const Vector2  _size, float _rotate, float _speed);
};