#pragma once
#include <d3d12.h>
#include <wrl/client.h>
#include <vector>
#include <array>
#include <cstddef>
#include "GfxType.h"
#include "../Math/TSMath.h"
#include "../Core/Handle/TexHandle.h"
#include "../Core/Handle/ModelHandle.h"
#include "../Core/Handle/ShaderHandle.h"
#include "../Core/Handle/MaterialHandle.h"
#include "GraphicsConstant.h"
using Microsoft::WRL::ComPtr;

// 描画関連で汎用的に使う型を定義する

	// ライブラリ内蔵Shaderを識別するID
enum class BuiltinShaderID : size_t
{
	ShapeVS,
	ShapePS,

	ModelVS,
	ModelPS,

	TextureVS,
	TexturePS,

	TerrainVS,
	TerrainHS,
	TerrainDS,
	TerrainPS,

	PostEffectVS,
	PostEffectPS,

	Count,

	// Shaderを使用しない場合用
	None = Count
};

// heapの種類
enum class HeapType
{
	CBV_SRV_UAV, // GPU可視
	RTV, // 描画先
	DSV, // 深度
};

// 規定のパイプラインステートを選ぶためのID
enum class PipelineID
{
	Sprite, // 画像
	Model, // 3Dモデル
	ShapeFill, //  2D基本図形塗りつぶし
	ShapeWire, // 2D基本図形ワイヤー
	TerrainWire, // テッセレーションデモ
	PostEffect, // シーンRTを画面へ描画するPSO
	Count,
};

// 規定のルートシグネチャを選ぶためのID
enum class RootSigID
{
	Texture, // 画像用
	Model, // 3Dモデル
	Shape, // 2D基本形状
	Terrain, // テッセレーションデモ
	PostEffect, // シーンRTのSRVのPSから読むためのルートシグネチャ
	Count,
};

// ブレンドモードの設定
enum class BlendMode
{
	Opaque, // 不透明
	Alpha,  // 透明度計算含み
	Count,
};

// セマンティクス設定を選択するためのもの
enum class InputLayout
{
	None, // なし(ポストエフェクト等に対応させるため)
	Texture, // 画像(position + uv)
	Sprite, // 画像(position + uv + color)
	Model, // 3Dモデル
	Shape, // 2D形状
	Count,
};

// 深度を表す
enum class DepthParam
{
	None, // 深度計算なし
	ReadWrite, // 読み込み書き込みができる
	ReadOnly, // 読み込みだけ
	Count,
};

// DescriptorTabel内の1レンジ
struct DescriptorRangeDesc
{
	D3D12_DESCRIPTOR_RANGE_TYPE type{ D3D12_DESCRIPTOR_RANGE_TYPE_SRV }; // リソースの種類
	UINT numDescriptors{ 1 }; // 個数
	UINT baseShaderRegister{ 0 }; // 開始レジスタ番号
	UINT registerSpace{ 0 }; // レジスタの空間区切り
	UINT offset{ D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND }; // オフセット
};

// ルートパラーメータ1つ
struct RootParamDesc
{
	D3D12_ROOT_PARAMETER_TYPE type{ D3D12_ROOT_PARAMETER_TYPE_CBV }; // 種類 : デフォルトはCBV
	D3D12_SHADER_VISIBILITY visibility{ D3D12_SHADER_VISIBILITY_ALL }; // アクセスできる範囲
	// CBV/SRV/UAV,Constant用
	UINT shaderRegister{ 0 };
	UINT registerSpace{ 0 };
	UINT num32BitValues{ 0 };

	// DescriptorTableの場合のみ使用
	std::vector<DescriptorRangeDesc> ranges{};
};

// RootSignature全体
struct RootSignatureDesc
{
	RootSigID rootSignatureID{RootSigID::Count}; // IDを無効値として設定する

	std::vector<RootParamDesc> parameters{}; // パラメータ
	std::vector<D3D12_STATIC_SAMPLER_DESC> staticSamplers{}; // サンプラー

	D3D12_ROOT_SIGNATURE_FLAGS flags{ D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT };
};

// GraphicsのPSO生成時に使う設定構造体
struct GraphicsPipelineDesc
{
	RootSigID rootSignatureID{ RootSigID::Count }; // ルートシグネチャの鍵
	PipelineID pipelineID{PipelineID::Count}; // パイプラインステートの鍵
	// 内蔵Shaderを指定する
	BuiltinShaderID  vs{ BuiltinShaderID::None }; // 頂点シェーダー
	BuiltinShaderID ps{ BuiltinShaderID::None }; // ピクセルシェーダー
	BuiltinShaderID hs{ BuiltinShaderID::None }; // ハルシェーダー
	BuiltinShaderID ds{ BuiltinShaderID::None }; // ドメインシェーダー
	BuiltinShaderID gs{ BuiltinShaderID::None }; // ジオメトリシェーダー
	InputLayout layout{InputLayout::None}; // 入力レイアウト
	BlendMode blend{BlendMode::Opaque}; // ブレンドモード
	DepthParam depth{DepthParam::None
	}; // 深度設定
	D3D12_PRIMITIVE_TOPOLOGY_TYPE topology{ D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE }; // 形状
	D3D12_FILL_MODE fillMode{ D3D12_FILL_MODE_SOLID };
};

// Compute用PSO生成時に使う設定構造体
struct ComputePipelineDesc
{
	RootSigID rootSignatureID{ RootSigID::Count }; // ルートシグネチャの鍵
	PipelineID pipelineID{}; // パイプラインステートの鍵
	const wchar_t* csPath{ nullptr }; // コンピュートシェーダーパス
};

// 書き込みを行うためのCPUハンドルと読み取るためのGPUハンドルとそのインデックスをまとめたハンドル
struct DescriptorHandle
{
	D3D12_CPU_DESCRIPTOR_HANDLE cpu{}; // CPUハンドル
	D3D12_GPU_DESCRIPTOR_HANDLE gpu{}; // GPUハンドル
	UINT index{ INVALID_INDEX }; // インデックス(デフォルトは無効値)

	// 無効値か確かめる関数
	bool IsValid() const { return index != INVALID_INDEX; }
};

// テクスチャの情報をまとめた構造体
struct TextureData
{
	ComPtr<ID3D12Resource> resource; // テクスチャリソース
	DescriptorHandle srvHandle; // ShaderReosurceViewハンドル
	int width{ 0 }; // 画像の横幅
	int height{ 0 }; // 画像の縦幅
};

// 頂点バッファ一つ分の情報をまとめた構造体
struct VertexBuffer
{
	ComPtr<ID3D12Resource> resource; // リソースオブジェクト
	D3D12_VERTEX_BUFFER_VIEW vertexView{}; // 頂点バッファビュー
	UINT sizeInBytes{ 0 }; // バッファ全体のサイズ
	void* mappedPtr{ nullptr }; // CPUハンドルを入れるマップしたポインタ
};

// インデックスバッファとしての情報をまとめた構造体
struct IndexBuffer
{
	ComPtr<ID3D12Resource> resource; // リソースオブジェクト
	D3D12_INDEX_BUFFER_VIEW indexView{}; // インデックスバッファビュー
	UINT indexCount{ 0 }; // インデックスの数
};

// 定数バッファを作成したら返ってくる構造体
struct ConstantBufferData
{
	ComPtr<ID3D12Resource> resource; // リソースオブジェクト
	void* mappedPtr{nullptr}; // マップしたポインタ(CPUハンドルを入れる)
	DescriptorHandle cbvHandle; // シェーダーにバインドするためのハンドル
};

// RootCBV等に使える汎用的なDescriptorを通さない構造体
struct DynamicBuffer
{
	ComPtr<ID3D12Resource> resource; // リソースオブジェクト
	void* mappedPtr{ nullptr }; // マップしたポインタ(CPUハンドル)
};

// 頂点定義
struct TexVertex
{
	float position[3]; // 座標
	float uv[2]; // uv座標 
};

// 頂点定義
struct SpriteVertex
{
	float position[3]; // 画面座標
	float uv[2];       // テクスチャUV
	float color[4];    // Tintカラーと透明度
};

// 頂点定義
struct ShapeVertex
{
	float position[3];
	float color[4];
};

// 管理するスロット(実体と世代で管理するResourceManagerにfreelistがあるので占有しているかのフラグはなし)
struct TextureSlot
{
	TextureData data; // 実体
	uint32_t generation{ 0 }; // 世代
};

// 頂点内のデータを定義する構造体(Vectorを付けるとalignasによりoffsetがずれるため使わない)
struct ModelVertex
{
	float position[3]; // 位置
	float normal[3]; // 法線
	float uv[2]; // uv
	float weight[4]; // ボーンの重み
	uint32_t bones[4]; // ボーン
};

namespace MaterialTex 
{
	// intの暗黙変換を行うため通常のenum
	enum
	{
		BaseColor,
		Normal,
		MetallicRoughness,
		Emissive,
		Count,
	};
}

// materialの定数バッファ
struct MaterialCB
{
	// パディング = 16byteに調整するための変数
	Vector4 baseColorFactor{ 1.0f, 1.0f, 1.0f, 1.0f }; // 拡散色(デフォルトは白)
	float metallic{ 1.0f }; //　金属度
	float roughness{ 1.0f }; // 粗さ
	float pad0{ 0.0f };
	float pad1{ 0.0f };
	Vector3 emissiveFactor{ 0.0f, 0.0f, 0.0f }; // 自己発光色
	float pad2{ 0.0f }; 
};

// material本体
struct Material
{
	TexHandle textures[MaterialTex::Count]; // テクスチャ群
	Vector4 baseColorFactor{ 1.0f, 1.0f, 1.0f, 1.0f }; // 拡散色(デフォルトは白)
	float metallic{ 1.0f }; //　金属度
	float roughness{ 1.0f }; // 粗さ
	Vector3 emissiveFactor{ 0.0f, 0.0f, 0.0f }; // 自己発光色
};

// サブメッシュ単位の構造体
struct SubMesh
{
	VertexBuffer vertexBuffer; // 頂点バッファ
	IndexBuffer indexBuffer; // インデックスバッファ(この中にIndexCountがあるためそれを使う)
	Material material; // マテリアル
};

// ボーン一つ分のデータを持つ
struct Bone
{
	int parentIndex{ -1 }; // 親ボーンのIndex(Rootは-1)
	Mat4x4 inverseBindMatrix; // IBM行列(gltfから読む。頂点をバインドポーズのボーン原点から見た位置へ戻す)
	Mat4x4 localPose; // ボーンのローカル姿勢行列(親から見た相対、アニメーションで更新される)

	// バインドポーズのTRS(animationされないボーンの初期値に使う)
	Vector3 bindTranslation{ Vector3::Zero };
	Quaternion bindRotation{ Quaternion::Identity };
	Vector3 bindScale{ Vector3::Zero };
};

// アニメーションのパスを明示的に出せるようにする名前空間
namespace AnimPath {
	enum
	{
		Translation ,
		Rotation,
		Scale
	};
}

// アニメーションの１本分のチャンネル
struct AnimChannel
{
	int boneIndex{ -1 }; // どのボーンか
	int path{ 0 }; // どの扱い方をするか
	std::vector<float> times; // 時刻配列(キーフレームの時刻)
	std::vector<Vector4> values; // 値配列(T/Sはxyz + あまり, Rはxyzw)
};

// アニメーション本体のデータ
struct Animation
{
	std::string name; // アニメーションの名前
	float duration{ 0.0f }; // アニメーションの長さ(時刻の最大値)
	std::vector<AnimChannel> channels; // アニメーションのチャンネル配列
};


// モデルそのものを構成する構造体
struct ModelData
{
	std::vector<SubMesh> subMeshes; // 構成するサブメッシュ
	std::vector<Bone> bones; // 構成するボーン
	std::vector<Animation> animations; // 構成するアニメーション
	Mat4x4 skeletonRoot{ Mat4x4::Identity }; // Armature変換用(ルートの親)
	std::vector<TexHandle> ownedTextures; // モデル読み込み時にこのモデル用としてReosurceManagerが用意したTexture群
};

// 個体ごとのアニメーションの状態
struct AnimInstanceData
{
	ModelHandle handle; // どのモデルかを判別するハンドル
	std::vector<Mat4x4> globalPoses; // この個体の現在のボーン姿勢
	std::vector<Mat4x4> skinningMatrices; // スキニング行列
	int currentAnim{ 0 }; // 現在のアニメーション
	float currentTime{ 0.0f }; // 再生時刻
}; 

// 管理するスロット
struct ModelSlot
{
	ModelData data; // 実体
	uint32_t generation{ 0 }; // 世代
};

// TerrainのCB
struct TerrainCB
{
	Mat4x4 mvp{ Mat4x4::Identity }; // mvp行列
	Vector4 color{ 1.0f, 1.0f, 1.0f, 1.0f }; // 色
	float heightScale{ 1.0f }; // 高さ具合
	float tessFactor{ 4.0f }; // 分割係数
	float padding[2]{};
};

// オフスクリーン描画先1個分
struct RenderTargetData
{
	ComPtr<ID3D12Resource> resource{}; // 画像を保持するgpuリソース
	DescriptorHandle rtvHandle{}; // RenderTargetとして書き込むview
	DescriptorHandle srvHandle{}; // Shaderから読み込むview
	UINT width{ 0 }; // 横幅
	UINT height{ 0 }; // 縦幅
};

// RenderTarget管理スロット
struct RenderTargetSlot
{
	RenderTargetData data{}; // 実データ
	uint32_t generation{ 0 }; // 世代
};

// Shader一つ分の実データ
struct ShaderData
{
	ShaderUsage usage{ ShaderUsage::PostEffect }; // 一旦ポストエフェクト
	ShaderStage stage{ ShaderStage::Pixel };
	ComPtr<ID3DBlob> blob{};
};

// Shaderを管理するスロット
struct ShaderSlot
{
	ShaderData data{};
	uint32_t generation{ 0 }; // 世代
};

// Materialユーザーパラメータの1Slot分
struct MaterialParameterBlock
{
	std::array<std::byte, MAX_MATERIAL_PARAMETER_SIZE> parameterData{}; // SetMaterialParameterで設定されたCPU側のパラメータ保管庫GPUへ送るまではここで保持
	size_t parameterSize{ 0 }; // ユーザーが渡した実際のデータサイズ
	bool hasParameter{ false }; // 一度でもパラメータが設定されたか
};

using MaterialParameterSet = std::array<MaterialParameterBlock, MATERIAL_PARAMETER_SLOT_COUNT>;
// material一つ分の実データ
struct MaterialData
{
	ShaderUsage usage{ ShaderUsage::PostEffect }; // Shaderがどの描画カテゴリだったか
	ComPtr<ID3D12PipelineState> pipelineState{}; // Shaderと用途ごとのPSO設定から生成したもの
	MaterialParameterSet parameters{}; // slot0-3のユーザーパラメータ
};

// materialを管理するスロット
struct MaterialSlot
{
	MaterialData data{};
	uint32_t generation{ 0 }; // 世代
};

// 1回のGPU送信に対応する解放待ちのリソース
struct DeferredReleaseBatch
{
	UINT64 fenceValue{ 0 }; // GPUがこの値まで完了したら解放可能
	std::vector<TextureData> textures{}; // SRVとTextureResourceを保持する
	std::vector<ModelData> models{}; // VB・IB・Material等を保持する
	std::vector<RenderTargetData> renderTargets{}; // RenderTargetのリソースとRTV/SRVを保持する
	std::vector<ComPtr<ID3D12PipelineState>> pipelineStates{}; // GPUが使用中かもしれないmaterialのPSO
};
