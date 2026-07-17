#include "Background.h"

void Background::Update()
{

}

void Background::Draw()
{
	if (!isActive) return;
	Gfx::DrawSprite(handle, position, size, rotate, { Vector2::Zero }, { Vector2::One }, LenderLayer::BackGround); // 背景として描画
}