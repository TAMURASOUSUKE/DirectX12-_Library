#pragma once
#include <cstddef>
#include <array>
#include <cstdint>
#include "../Core/Handle/MaterialHandle.h"
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
	VFX,
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

// ビルボードの状態を表す列挙体
enum class BillboardMode
{
	None, // 向きはTransformに合わせる
	FaceCamera, // 常にカメラ方向を向く
	AxisY, // Y軸を固定して水平方向だけカメラを向く
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

// VFXに使われる頂点の情報をまとめた型
struct VFXVertex
{
	float position[3]{ 0.0f, 0.0f, 0.0f }; // ワールド位置
	float uv[2]{ 0.0f, 0.0f }; // UV
	float color[4]{ 1.0f, 1.0f, 1.0f, 1.0f }; // 頂点色(αは不透明度または加算の強さ)
};
static_assert(sizeof(VFXVertex) == sizeof(float) * 9, "VFXVertexは詰め物なしの36バイトである必要があります(入力レイアウトと一致させる)");

// パーティクルの更新で行うパラメータ
struct alignas(16) ParticleUpdateParameter
{
	float emitPosition[3]{ 0.0f, 0.0f, 0.0f }; // 発生地点
	float deltaTime{ 0.0f }; // 更新時間
	float gravity[3]{ 0.0f, -9.8f, 0.0f }; // 重力
	std::uint32_t emitCount{ 0 }; // 生成数
	std::uint32_t particleCount{ 0 }; // パーティクルの数
	std::uint32_t randomSeed{ 0 }; // ランダム
	float lifeMin{ 0.5f }; // 寿命の最短
	float lifeMax{ 1.5f }; // 寿命の最長
	float speedMin{ 5.0f }; // スピードの下限
	float speedMax{ 12.0f }; // スピードの上限
	float size{ 0.1f }; // サイズ
	float drag{ 0.6f }; // 空気抵抗 減衰率
	float color[4]{ 1.0f, 0.6f, 0.2f, 1.0f }; // 色
	float emitVelocity[3]{ 0.0f, 0.0f, 0.0f }; // 生成した瞬間の速度ベクトル
	float swirl{ 0.0f }; // 粒子の渦巻などの回転運動の強さ
	float coneDirection[3]{ 0.0f, 1.0f, 0.0f }; // 円錐状に出す際のベクトル
	float coneBias{ 0.8f }; // 円錐の広がり具合
	float spawnRadius{ 0.0f }; // 発生する範囲の半径
	float groundY{ -1.0e6f }; // 接地判定を行うY座標
	float bounce{ 0.0f }; // 地面に衝突したときの跳ね返り
	float sizeSpread{ 0.4f }; // サイズのばらつき具合
};
static_assert(sizeof(ParticleUpdateParameter) == 128, "ParticleUpdateParameter must match the HLSL cbuffer (128 bytes)");

constexpr std::uint32_t VFX_PARTICLE_STRIDE{ 64 }; // Particle.hlsli の Particle 1個の大きさ
constexpr std::uint32_t VFX_PARTICLE_THREAD_PER_GROUP{ 256 }; // 更新CSの numthreads
// VFXのパーティクルの設定
struct VFXParticleDesc
{
	MaterialHandle material; // 使用するmaterial
	ComputePipelineHandle updatePipeline; // 更新用のPSO
	ComputePipelineHandle resetPipeline; // パーティクルをリセットするためのPSO
	std::uint32_t particleCount{ 0 };   // 枠1つあたりの粒子数
	std::uint32_t maxInstances{ 0 };    // 同時再生数
	float duration{ 0.0f };  // 発生を続ける秒数
	std::uint32_t emitPerFrame{ 0 };   // 発生中の毎フレーム発生数
	ParticleUpdateParameter parameter; // 再生中に変わらない部分（重力・寿命・速度・色など）
};

// 再生管理で使うMeshの形状
enum class VFXMeshKind
{
	Quad, // 板
	Trail, // 帯
};

// VFXのメッシュの種類設定
struct VFXMeshDesc
{
	MaterialHandle material; // 使用するmaterial
	VFXMeshKind kind{ VFXMeshKind::Quad }; // 板か帯か
	BillboardMode billboardMode{ BillboardMode::None }; // 板の向きの決め方(帯では使わない)
	float width{ 1.0f }; // 板の幅
	float height{ 1.0f }; // 板の高さ
	float duration{ 0.0f }; // 再生する秒数
	std::uint32_t maxInstances{ 0 }; // 同時再生数
	std::uint32_t trailPointCount{ 0 }; // 帯が覚えておく点の数(板では使わない)
	float trailPointLife{ 0.0f }; // 帯の点が消えるまでの秒数(板では使わない)
};

constexpr std::size_t VFX_TIME_PARAMETER_SLOT{ 3 }; // ライブラリが時間を書くMaterialParameterのslot

// ライブラリが再生中のMaterialへ毎フレーム書く値
struct VFXTimeParameter
{
	float progress{ 0.0f }; // 進み具合(0で開始1で終了)
	float elapsed{ 0.0f }; // 開始からの経過時間
	float duration{ 0.0f }; // 再生する秒数
	float padding{ 0.0f };
};

static_assert(sizeof(VFXTimeParameter) == 16, "VFXTimeParameterはHLSLのcbufferと同じ16バイトである必要があります");
