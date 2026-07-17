#pragma once
#include"../Math/TSMath.h"
// 判定形状を設定する


// 矩形
struct Rect
{
	Rect(Vector2 _leftTop, Vector2 _rightBottom) : leftTop{ _leftTop }, rightBottom{ _rightBottom }
	{
		rightTop = { leftTop.x + rightBottom.x, leftTop.y };
		leftBottom = { leftTop.x, leftTop.y + rightBottom.y };
		center = { leftTop + rightBottom / 2.0f };
	}

	Vector2 leftTop{}; // 左上
	Vector2 rightTop{}; // 右上
	Vector2 leftBottom{}; // 左下
	Vector2 rightBottom{}; // 右下
	Vector2 center{}; // 中心
};