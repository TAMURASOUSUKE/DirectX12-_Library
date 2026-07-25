#include "Background.h"

void Background::Update()
{

}

void Background::Draw()
{
	if (!isActive) return;
	Gfx::DrawSpriteSized(handle, position, size, rotate, { Vector2::Zero }, { Vector2::One }, RenderLayer::BackGround); // 背景として描画
}