#pragma once
#include <d3d12.h>
#include <array>
#include <vector>
#include <span>
#include "GfxType.h"
#include "GraphicsType.h"
#include "GraphicsConstant.h"
#include "VFXDrawPacket.h"
#include "RingConstantBuffer.h"
#include "../Core/Handle/MaterialHandle.h"
#include "../Math/Matrix/Mat4x4.h"
#include "../Math/Vector/Vector3.h"

// VFXメッシュ(板・帯)の頂点をフレーム単位で預かり、カメラから遠い順にMaterialつきで描画する
class VFXMeshSystem
{
public:
	VFXMeshSystem() = default;
	~VFXMeshSystem() = default;

	VFXMeshSystem(const VFXMeshSystem&) = delete;
	VFXMeshSystem& operator=(const VFXMeshSystem&) = delete;

	// VFX用RootSignature(非所有)を受け取り、フレームごとの頂点領域と定数バッファを作る
	bool Setup(ID3D12RootSignature* _rootSignature);

	// GPUリソースとCPU側の登録情報を解放する
	void Shutdown();

	// フレーム先頭で呼ぶ 書き込み位置と登録を戻し、未設定スロット用のゼロ番地を作り直す
	void Reset();

	// 1メッシュ分の頂点(TRIANGLESTRIP、3頂点以上)をMaterialつきで登録する 上限超過・不正時はfalse
	bool Register(std::span<const VFXVertex> _vertices, MaterialHandle _material);

	// 頂点を持たない描画単位を登録する
	bool RegisterInstanced(MaterialHandle _material, UINT _instanceCount, const Vector3& _sortPosition);

	// 登録済みメッシュをカメラから遠い順に描画する(半透明Modelの後に呼ぶ)
	bool Flush(const Mat4x4& _viewProjection, const Vector3& _cameraPosition, const Quaternion& _cameraRotation);

	// 板の右と上の向きをモードから決める 不明なモードならfalse
	static bool ComputeQuadAxes(BillboardMode _mode, const Quaternion& _rotation, const Vector3& _center, const Quaternion& _cameraRotation, const Vector3& _cameraPosition, Vector3& _outRight, Vector3& _outUp);

	// 計算用の型からGPUへ送る詰め物なしの頂点を作る
	static VFXVertex MakeVertex(const Vector3& _position, float _u, float _v, const Vector4& _color);

private:
	static constexpr UINT VERTEX_STRIDE{ static_cast<UINT>(sizeof(VFXVertex)) }; // 1頂点のバイト数
	static constexpr UINT MATERIAL_PARAMETER_ROOT_PARAM_BASE{ 1 }; // RootParameter[1]〜[4] = b0〜b3 space1(SetMaterialParameter)
	static constexpr UINT MATERIAL_TEXTURE_ROOT_PARAM_BASE{ 5 }; // RootParameter[5]〜[8] = t0〜t3 space1(SetMaterialTexture)

	ID3D12RootSignature* rootSignature{ nullptr }; // 非所有 ShaderSystemのVFX用
	RingConstantBuffer frameRingCBV{}; // VFXFrameCB(viewProjection) 1フレーム1回
	RingConstantBuffer userMaterialParameterRingCBV{}; // SetMaterialParameterの値を描画ごとに配達する
	D3D12_GPU_VIRTUAL_ADDRESS zeroMaterialParameterAddress{ 0 }; // 未設定スロットに結びつけるゼロ埋めの番地 Resetで毎フレーム作り直す

	std::array<DynamicBuffer, FRAME_BUFFER_COUNT> vertexBuffers{}; // GPUが前フレームを読んでいる間にCPUが上書きしないようフレーム番号で交互に使うMap済み頂点領域
	UINT writtenVertexCount{ 0 }; // 現在フレームに書いた頂点数(=次の書き込み位置)
	std::size_t droppedVertexCount{ 0 }; // 上限超過で捨てた頂点数
	std::size_t droppedPacketCount{ 0 }; // 上限超過で捨てたメッシュ数

	std::vector<VFXDrawPacket> drawPackets{}; // 登録順 Flushでカメラ距離順に並べ替える
	bool isInitialized{ false }; // Setup済みか
};
