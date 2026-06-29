#pragma once
#include <d3d12.h>
#include <wrl/client.h>
#include <vector>
#include "../Math/TSMath.h"
#include "../Core/Handle/TexHandle.h"
#include "../Core/Handle/ModelHandle.h"
#include "GraphicsConstant.h"
using Microsoft::WRL::ComPtr;

// 描画関連で汎用的に使う型を定義する(ハンドルなど)

// heapの種類
enum class HeapType
{
	CBV_SRV_UAV, // GPU可視
	RTV, // 描画先
	DSV, // 深度
};

// 描画順を決定する際に選べる種類
enum class LenderLayer
{
	BackGround, // 背景
	ForeGround, // 3Dオブジェクトより手前に来る画像
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
struct ColorVertex
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
	Vector4 baseColorFactor{ 1.0f, 0.0f, 0.0f, 1.0f }; // 拡散色(デフォルトは白)
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