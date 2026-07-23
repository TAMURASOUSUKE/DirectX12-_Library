#pragma once
#include <functional> // 規模が小さめなのでとりあえずfunctionalで対応する
#include <utility>
#include "../../Src/Facade/TSLib.h"
#include "ObjectBase.h"

// プレイヤーを定義する
class Player : public ObjectBase
{
public:
	// ゴール到達時に呼び出す処理
	using GoalCallback = std::function<void()>;
	using LifeChangedCallback = std::function<void(int)>; // life変化時に呼び出す
	Player(TexHandle _handle, const Vector2& _position, const Vector2  _size, float _rotate, float _speed, GoalCallback _onGoal, LifeChangedCallback _onLifeChange) : ObjectBase(_handle, _position, _size, _rotate), speed{ _speed }, onGoal{std::move(_onGoal)}, onLifeChange(std::move(_onLifeChange)) {}

	void Update() override;
	void Draw() override;

	ObjectType GetObjectType() const override
	{
		return ObjectType::Player;
	}

	void OnCollision(ObjectBase& _other) override;

	int GetLife() const { return life; }

private:
	GoalCallback onGoal{};
	LifeChangedCallback onLifeChange{};
	float speed{ 0.0f };
	int life{ 3 };
};