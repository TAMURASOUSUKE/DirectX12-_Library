#pragma once
#include <d3d12.h>
#include "../Core/Handle/MaterialHandle.h"
#include "GraphicsType.h"

// パケット転送の際に無駄な転送を避けるためGPUアドレスを共有するための構造体
struct PreparedModelDrawData
{
	D3D12_GPU_VIRTUAL_ADDRESS objectAddress{ 0 };
	D3D12_GPU_VIRTUAL_ADDRESS skinningAddress{ 0 };
};

// ModelRenderSystemから組み立てられたGPUへ送る描画単位
struct ModelDrawPacket
{
	const SubMesh* subMesh{ nullptr }; // マテリアルやIB,VBを参照する描画対象

	PipelineID pipelineID{ PipelineID::Count }; // OpaqueかBlendか片面か両面かの組み合わせを事前に決定する
	float sortDepth{ 0.0f }; // 半透明、LOD、距離カリングで使うカメラ距離
	PreparedModelDrawData preparedData{}; // 共有するアドレス
	MaterialHandle effectiveMaterial{}; // 最終的に適用されるmaterial
};
