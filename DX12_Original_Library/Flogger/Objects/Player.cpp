#include "Player.h"

void Player::Update()
{
	if (!isActive) return;
	Vector2 dir{Vector2::Zero};
	if (Input::IsKeyPress(KeyCode::Button::W)) dir.y -= 1.0f;
	if (Input::IsKeyPress(KeyCode::Button::S)) dir.y += 1.0f;
	if (Input::IsKeyPress(KeyCode::Button::A)) dir.x -= 1.0f;
	if (Input::IsKeyPress(KeyCode::Button::D)) dir.x += 1.0f;
	dir.Normalize();

	position += dir * speed * Time::DeltaTime();

	if (position.y < 0.0f)
	{
		// ゴールした瞬間だけゲーム側へ通知する
		if (onGoal)
		{
			onGoal();
		}

		position = { 629.0f, 632.0f };
	}
}

void Player::Draw()
{
	if (!isActive) return;
	Gfx::DrawSpriteSized(handle, position, size, rotate);
}

void Player::OnCollision(ObjectBase& _other)
{
	// 車以外との衝突ではライフを減らさない
	if (_other.GetObjectType() != ObjectType::Car)
	{
		return;
	}

	--life;

	// main側へ現在のライフを通知する
	if (onLifeChange)
	{
		onLifeChange(life);
	}

	position = { 629.0f, 632.0f };
}