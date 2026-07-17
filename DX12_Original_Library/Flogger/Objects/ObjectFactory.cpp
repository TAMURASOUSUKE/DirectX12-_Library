#include <utility>
#include <memory>
#include "ObjectFactory.h"
#include "ObjectManager.h"

// 具体
#include "Background.h"
#include "Car.h"
#include "Player.h"

void ObjectFactory::CreateBackground(TexHandle _handle, const Vector2& _position, const Vector2  _size, float _rotate)
{
	std::unique_ptr<ObjectBase> obj{ std::make_unique<Background>(_handle, _position, _size, _rotate) };
	ObjectManager::Instance().Register(std::move(obj));
}

void ObjectFactory::CreatePlayer(TexHandle _handle, const Vector2& _position, const Vector2  _size, float _rotate, float _speed, std::function<void()> _onGoal, std::function<void(int)> _onLifeChange)
{
	std::unique_ptr<ObjectBase> obj{ std::make_unique<Player>(_handle, _position, _size, _rotate, _speed, std::move(_onGoal), std::move(_onLifeChange)) };
	ObjectManager::Instance().Register(std::move(obj));
}

void ObjectFactory::CreateCar(TexHandle _handle, const Vector2& _position, const Vector2  _size, float _rotate, float _speed)
{
	std::unique_ptr<ObjectBase> obj{ std::make_unique<Car>(_handle, _position, _size, _rotate, _speed) };
	ObjectManager::Instance().Register(std::move(obj));
}