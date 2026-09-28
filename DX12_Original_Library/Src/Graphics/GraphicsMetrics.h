#pragma once
#include <cstdint>
#include <cstddef>
#include <array>

// どのパスでカウントするかを分ける
enum class GraphicsPass : std::uint8_t
{
	BackgroundSprite,
	Shadow,
	OpaqueModel,
	Primitive3D,
	BlendModel,
	ForegroundSprite,
	Shape2D,
	DebugPreview,
	PostEffect,
	Terrain,
	Count
};

// 完成した1フレーム分の描画統計
struct GraphicsFrameMetrics
{
	std::uint64_t drawCallCount{ 0 }; // 合計のドローコール
	std::array<std::uint64_t, static_cast<std::size_t>(GraphicsPass::Count)> passDrawCallCount{}; // パスごとのドローカウント
};

// Graphcis内部だけが書き込み、外部は完成値だけ読む名前空間
namespace GraphicsMetrics
{
	void BeginFrame();
	void EndFrame();

	// 実際にDraw命令を1回発行するときに呼ぶ
	void RecordDrawCall(GraphicsPass _pass);

	// 完成済みの前フレーム値を取得する
	const GraphicsFrameMetrics& GetLastFrame();
}
