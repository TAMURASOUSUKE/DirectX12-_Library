#pragma once
#include <d3d12.h>
#include <wrl/client.h>
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
	float position[3];
	float normal[3];
	float uv[2];
};