#include "Car.h"

void Car::Update()
{
	if (!isActive) return;

	position.x += speed * Time::DeltaTime();

	if (speed >= 0.0f)
	{
		// 右向き
		if (position.x > 1280.0f) position.x = -size.x; // 赤い車が完全に隠れる位置へ
	}
	else
	{
		if (position.x < -size.x) position.x = 1280.0f + size.x;
	}
}

void Car::Draw()
{
	if (!isActive) return;
	Gfx::DrawSprite(handle, position, size, rotate);
}