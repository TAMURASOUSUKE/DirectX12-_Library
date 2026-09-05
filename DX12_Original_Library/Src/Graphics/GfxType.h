#pragma once
#include <cstddef>
#include "../Core/Handle/ModelHandle.h"

// 描画するスプライトのレイヤー
enum class RenderLayer
{
	BackGround, // 3D描画より後ろ
	ForeGround, // 3D描画より手前
};

// モデルマテリアルの透明表現
enum class ModelAlphaMode
{
	Opaque, // 完全不透明
	Mask, // AlphaCutoffを基準に描画するか完全破棄する
	Blend, // 背景色とAlphaで混ぜる
};

// シェーダーを適用する描画用途
enum class ShaderUsage
{
	PostEffect, // 対応済み
	Sprite, //　対応済み
	Model, // 3D機能整備時に対応
};

// シェーダーステージの種類
enum class ShaderStage
{
	Vertex, // 公開materialで対応済み
	Pixel, // 公開materialで対応済み
	// 以下はShaderとしてコンパイル可能ではあるが現在のCreateMaterialではまだ使用不可
	Hull,
	Domain,
	Geometry,
	Compute,
};

// 1段階分のLOD設定
struct ModelLODLevel
{
	float minDistance{ 0.0f };
	// この段階で描画するモデル
	ModelHandle model{};
};

// 一つの描画対象が前フレームまで使用していたLODを保持(ヒステリシスを行うための閾値を決定づけるために必要)
struct ModelLODState
{
	std::size_t currentLevelIndex{ 0 }; // 現在選択してるLOD
	bool isInitialized{ false }; // 初回は過去のLODがないので通常の距離判定を行う
	void Reset()
	{
		currentLevelIndex = 0;
		isInitialized = false;
	}
};

// MaterialParameterの1スロットへ保存できる最大サイズ
inline constexpr std::size_t MAX_MATERIAL_PARAMETER_SIZE{ 256 };


