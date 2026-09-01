#pragma once
#include <vector>
#include <span>
#include <variant>
#include "GraphicsConstant.h"
#include "../Core/Handle/ModelHandle.h"
#include "../Component/Transform.h"
#include "../Core/Handle/AnimInstanceHandle.h"

// 静的モデル一つ分の描画コマンド
struct StaticModelRenderCommand
{
	ModelHandle modelHandle{};
	Transform transform{};
};

// スキニングモデル一つ分の描画コマンド
struct SkinningModelRenderCommand
{
	AnimInstanceHandle animInstanceHandle{};
	Transform transform{};
};

// 静的モデルまたはスキニングモデルのどちらか一方を保持する
using ModelRenderCommand = std::variant<StaticModelRenderCommand, SkinningModelRenderCommand>;

// 1フレームのモデル描画依頼を保存
class ModelRenderQueue
{
public:


private:

};
