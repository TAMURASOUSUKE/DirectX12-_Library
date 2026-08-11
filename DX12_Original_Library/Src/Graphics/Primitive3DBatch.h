#pragma once
#include <span>
#include <array>
#include <vector>
#include <cstddef>
#include <d3d12.h>
#include "RingConstantBuffer.h"
#include "GraphicsType.h"
#include "GraphicsConstant.h"

// Primitive3DBatchが所有する単位メッシュの識別子
enum class Primitive3DMeshID : std::size_t
{
	Cube,
	Sphere,
	Cylinder,
	Hemisphere, // capsuleの両端用
	Plane,
	Line,
	Count
};

// 塗りつぶしかワイヤーフレームかを識別する
enum class Primitive3DDrawMode : std::size_t
{
	Fill,
	MeshWireframe,
	DebugLine,
	Count
};

// 同じメッシュ・同じ描画方法のインスタンスをまとめる箱
struct Primitive3DInstanceBucket
{
	std::vector<Primitive3DInstanceData> instances{};
};

// GPUバッファ内で各Bucketが配置された範囲
struct Primitive3DInstanceRange
{
	UINT startInstance{ 0 };
	UINT instanceCount{ 0 };
};

// 1種類の単位メッシュが所有するGPUリソース
struct Primitive3DMesh
{
	VertexBuffer vertexBuffer{};
	IndexBuffer triangleIndexBuffer{}; // FillとMeshWireFrameで使う三角形用IB
	IndexBuffer debugLineIndexBuffer{}; // あたり判定可視化で使う線分ようIB

	// それぞれのIBでのチェック
	bool HasTriangleGeometry() const { return vertexBuffer.resource && triangleIndexBuffer.resource && triangleIndexBuffer.indexCount > 0; }
	bool HasDebugLineGeometry() const { return vertexBuffer.resource && debugLineIndexBuffer.resource && debugLineIndexBuffer.indexCount > 0; }
};

// Batchへ渡す一個分の登録命令
struct Primitive3DRegistration
{
	Primitive3DMeshID meshID{};
	Primitive3DInstanceData instance{};
	Primitive3DDrawMode drawMode{};
};

// インスタンシングを使った3D基礎図形描画を管理する
class Primitive3DBatch
{
public:
	// RootSignatureとPSOを受け取りフレームごとのインスタンスバッファを作成
	bool Setup(ID3D12RootSignature* _rootSig, ID3D12PipelineState* _fillPipeLineState, ID3D12PipelineState* _wirePipeLineState, ID3D12PipelineState* _debugLineState);
	// GPUリソースとCPU側の登録情報を解放する
	void Shutdown();

	// 1個分の基礎図形を対応するBucketへ登録する
	bool Register(Primitive3DMeshID _meshID, const Primitive3DInstanceData& _instance, Primitive3DDrawMode _drawMode);
	// フレーム開始時に登録状態をリセットする
	void Reset(); 

	// 複数の図形を全部成功または全部失敗で登録する(中途半端な状態を作らない)
	bool RegisterGroup(std::span<const Primitive3DRegistration> _registration); // spanを使うことで連続したメモリ領域をコピーせず受け取る

	// 登録済みのインスタンスを描画する
	bool Flush(const Mat4x4& _viewProjection, D3D12_GPU_VIRTUAL_ADDRESS _sceneLightAddress);

private:
	// CPU側Bucketを現在フレームのGPUバッファへ連続コピーする
	bool UploadCurrentFrameInstances();

	// 単位Cubeの静的VB・IBを作成する
	bool CreateCubeMesh();
	// 緯度経度方式で単位Sphereを作成
	bool CreateSphereMesh();
	// 単位Clyinderを作成する
	bool CreateCylinderMesh();
	// capsuleの両端で使う上半球を用意
	bool CreateHemisphereMesh();
	// 平面に1x1の単位Planeを作成
	bool CreatePlaneMesh();
	// 長さ1の単位Lineを作成する
	bool CreateLineMesh();

private:
	static constexpr std::size_t MESH_COUNT{ static_cast<std::size_t>(Primitive3DMeshID::Count) };
	static constexpr std::size_t DRAW_MODE_COUNT{ static_cast<std::size_t>(Primitive3DDrawMode::Count) };

	std::array<DynamicBuffer, FRAME_BUFFER_COUNT> instanceBuffers{}; // GPUが前フレームを読んでいる時に上書きしないようにバックバッファごと独立した領域
	std::array<std::array<Primitive3DInstanceBucket, MESH_COUNT>, DRAW_MODE_COUNT> buckets{}; // 描画方法とメッシュで個体を分ける
	std::array<std::array<Primitive3DInstanceRange, MESH_COUNT>, DRAW_MODE_COUNT> ranges{}; // Upload後のGPUバッファ内の配置範囲
	std::array<Primitive3DMesh, MESH_COUNT> meshes{}; // Cube.Sphereなどの単位メッシュ
	std::array<ID3D12PipelineState*, DRAW_MODE_COUNT> pipelines{};

	std::size_t registeredInstanceCount{ 0 };
	std::size_t droppedInstanceCount{ 0 };

	ID3D12RootSignature* rootSignature{ nullptr };

	RingConstantBuffer frameConstantBuffer{}; // 全Primitive3Dで共有するviewProjection用CB
};
