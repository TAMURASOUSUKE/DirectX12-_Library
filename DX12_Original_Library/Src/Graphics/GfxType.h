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

// モデルのglTFマテリアルの透明表現
enum class ModelAlphaMode
{
	Opaque, // 完全不透明
	Mask, // AlphaCutoffを基準に描画するか完全破棄する
	Blend, // 背景色とAlphaで混ぜる
};

// ユーザーが作るMaterialのPSOを組む時の設定
enum class MaterialBlendMode
{
	Opaque, // 不透明
	Mask, // 閾値以下は破棄
	Blend, // 背景とAlphaで混ぜる
	Additive, // 加算合成を行う
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

// 出力をTextureで使う際にFormatを選ぶための列挙
enum class TextureFormat
{
	RGBA8_UNORM,  // RGBA各成分が8bitの符号なし正規化整数
	RGBA16_FLOAT, // 各成分が16bitの浮動小数点
	R32_FLOAT, // Rの成分だけ持っている32bitの浮動小数点
};

// フォントアトラスの設定構造体(デフォルトでフォントを用意しているが変更したい時にここを設定してもらう)
struct BitmapFont
{
	TexHandle texture; // アトラス画像のハンドル
	int texWidth{ 0 }; // テクスチャ全体の幅(px)
	int texHeight{ 0 }; // テクスチャ全体の高さ(px)
	int cellWidth{ 0 }; // 1セルの幅(px)
	int cellHeight{ 0 }; // 1セルの高さ(px)
	int cols{ 0 }; // 1行あたりのセルの量
	int firstCode{ 0 }; // 先頭セルが表す文字コード(CP437配列なら0, スペース始まりなら32)
};

// 画像の状態
enum class SpriteFlip
{
	None,
	Horizontal,
	Vertical,
	Both
};

// 等間隔のグリッド状に分割されたテクスチャアトラス
struct TextureAtlas
{
	TexHandle texture{}; // アトラス画像本体
	int columns{ 0 };    // 横方向の分割数
	int rows{ 0 };       // 縦方向の分割数
	int frameCount{ 0 }; // 実際に使用するセル数

	bool IsValid() const
	{
		const long long capacity{ static_cast<long long>(columns) * rows };
		return texture.IsValid() && columns > 0 && rows > 0 && frameCount > 0 && frameCount <= capacity;
	}
};

// スプライトアニメーションの設定と再生状態
struct SpriteAnimationState
{
	int firstFrame{ 0 };   // 使用する最初のセル
	int lastFrame{ 0 };    // 使用する最後のセル（この番号を含む）
	int currentFrame{ 0 }; // 現在表示しているセル

	float secondsPerFrame{ 0.1f }; // 1コマの表示秒数
	float elapsedTime{ 0.0f };     // コマ送り用の蓄積時間

	bool isLoop{ true }; // ループするか
	bool isFinished{ false }; // アニメーションが終了したか

	SpriteAnimationState() = default;

	// 必要な設定を引数だけで指定する
	SpriteAnimationState(int _firstFrame, int _lastFrame, float _secondsPerFrame, bool _isLoop = true) :
		firstFrame{ _firstFrame }, lastFrame{ _lastFrame }, currentFrame{ _firstFrame }, secondsPerFrame{ _secondsPerFrame }, isLoop{ _isLoop }
	{}

	bool IsValid(const TextureAtlas& _atlas) const
	{
		return _atlas.IsValid()
			&& firstFrame >= 0
			&& lastFrame >= firstFrame
			&& lastFrame < _atlas.frameCount
			&& currentFrame >= firstFrame
			&& currentFrame <= lastFrame
			&& secondsPerFrame > 0.0f;
	}

	int GetFrameIndex() const { return currentFrame; }

	void Reset()
	{
		currentFrame = firstFrame;
		elapsedTime = 0.0f;
		isFinished = false;
	}
};

// 画像をどの用途として読み込むか
enum class TextureUsage
{
	Color, // 表示用カラー画像(sRGBとして読み込む) 
	Data, // ハイトマップやノーマルマップなどの数値データ,Linerとして読み込む	
};

// 3Dの基本図形の描画方法を設定する
enum class Primitive3DStyle
{
	Fill, // 塗りつぶし
	MeshWireframe, // メッシュをワイヤー表示
	DebugLine, // あたり判定可視化などデバッグ途用として余計な線を描画しないワイヤー表示
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

// 出力UAVをTextureにする際の設定
struct TextureDesc
{
	int width{ 0 }; // 画像の横幅
	int height{ 0 }; // 画像の縦幅
	TextureFormat format{ TextureFormat::RGBA8_UNORM }; // 使用するformat
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

// PSO作成時に使われるMaterialの設定
struct MaterialRenderDesc
{
	MaterialBlendMode blendMode{ MaterialBlendMode::Opaque }; // 色をどう混ぜ合わせるか
	bool doubleSided{ false }; // 両面を描画するか
};
