#pragma once
#include <vector>
#include <span>
#include <variant>
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
	ModelRenderQueue() = default;
	~ModelRenderQueue() = default;

	ModelRenderQueue(const ModelRenderQueue& _other) = delete;
	ModelRenderQueue& operator=(const ModelRenderQueue& _other) = delete;

	// 描画依頼を保存するCPU領域を準備
	void Setup();
	
	// 保存領域も含め解放する
	void Shutdown();

	// 前フレームの登録情報を消して次のフレームに備える
	void Reset();

	// 静的モデルの描画依頼登録
	bool Register(ModelHandle _model, const Transform& _transform);
	// スキニングモデルの描画依頼登録
	bool Register(AnimInstanceHandle _animInstance, const Transform& _transform);

	// 登録された描画依頼をコピーせず参照する
	std::span<const ModelRenderCommand> GetCommands() const { return commands; }
	
	// 現在の登録数
	std::size_t GetCommandCount() const { return commands.size(); }

private:
	std::vector<ModelRenderCommand> commands{}; // 描画一つ分のコマンド群
	std::size_t droppedCommandCount{ 0 }; // 描画登録制限からあふれた数

};
