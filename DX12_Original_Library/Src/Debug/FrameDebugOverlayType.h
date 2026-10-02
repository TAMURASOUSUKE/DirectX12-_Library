#pragma once
#include <string>
#include <vector>
#include "../Math/Vector/Vector2.h"
#include "../Math/Vector/Vector4.h"

// 描画を担当する分野に依頼する際のまとまりを定義する

// 文字列をどのように描画するかを表す構造体
struct DebugTextCommand
{
	std::string text{};
	Vector2 position{};
	float scale{ 1.0f };
	Vector4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
};

// 1フレーム分のDebug描画命令
struct DebugOverlayFrame
{
	std::vector<DebugTextCommand> textCommands{};
};
