#pragma once
#include <d3d12.h>
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;

// 描画関連で汎用的に使う型を定義する(ハンドルなど)

// heapの種類
enum class HeapType
{
	CBV_SRV_UAV, // GPU可視
	RTV, // 描画先
	DSV, // 深度
};

// 書き込みを行うためのCPUハンドルと読み取るためのGPUハンドルとそのインデックスをまとめたハンドル
struct DescriptorHandle
{
	D3D12_CPU_DESCRIPTOR_HANDLE cpu{}; // CPUハンドル
	D3D12_GPU_DESCRIPTOR_HANDLE gpu{}; // GPUハンドル
	UINT index{ 0 }; // インデックス
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