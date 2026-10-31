#pragma once
#include <cstddef>
#include <array>
#include <cstdint>
#include "../Core/Handle/GPUBufferHandle.h"
#include "../Core/Handle/TexHandle.h"
#include "../Core/Handle/ComputePipelineHandle.h"
#include "../Core/Handle/ModelHandle.h"

// MaterialParameterの1スロットへ保存できる最大サイズ
inline constexpr std::size_t MAX_MATERIAL_PARAMETER_SIZE{ 256 };

// 一回のComputeで指定できるリソース数
inline constexpr std::size_t MAX_COMPUTE_SRV_COUNT{ 4 };
inline constexpr std::size_t MAX_COMPUTE_UAV_COUNT{ 4 };

// 一回のComputeへ渡せる定数データの最大サイズ
inline constexpr std::size_t MAX_COMPUTE_PARAMETER_SIZE{ 256 };

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
	PostEffect, 
	Sprite, 
	Model, 
	Compute,
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

// GPUで使用するStructuredBufferの設定
struct GPUBufferDesc
{
	std::size_t elementCount{ 0 }; // 格納する要素数
	std::size_t elementStride{ 0 }; // 1要素のサイズ(byte)
	bool allowUnorderedAccess{ false }; // UAVによる書き込みを許可するか
};

// Computeへ接続するリソースの種類
enum class ComputeResourceType
{
	None, // 未設定
	Buffer,
	Texture,
};

// Computeへ接続するリソース一つ分
struct ComputeResourceBinding
{
	ComputeResourceBinding() = default;

	// 指定したリソースの種類によってタイプが自動的に決定されるコンストラクタ
	ComputeResourceBinding(GPUBufferHandle _handle) : type{ ComputeResourceType::Buffer }, buffer{ _handle } {} // Buffer
	ComputeResourceBinding(TexHandle _handle) : type{ ComputeResourceType::Texture }, texture{ _handle } {} // Texture

	ComputeResourceType type{ ComputeResourceType::None }; // 接続するリソースの種類
	GPUBufferHandle buffer{};
	TexHandle texture{};
};

// 一回のCompute実行に必要な設定
struct ComputeDispatchDesc
{
	ComputePipelineHandle pipeline{};

	// 配列番号をHLSL側のレジスタ番号へ対応させる
	std::array<ComputeResourceBinding, MAX_COMPUTE_SRV_COUNT> inputs{}; // t0から
	std::array<ComputeResourceBinding, MAX_COMPUTE_UAV_COUNT> outputs{}; // u0から

	// 起動するスレッドグループ数
	std::uint32_t groupCountX{ 1 };
	std::uint32_t groupCountY{ 1 };
	std::uint32_t groupCountZ{ 1 };
};
