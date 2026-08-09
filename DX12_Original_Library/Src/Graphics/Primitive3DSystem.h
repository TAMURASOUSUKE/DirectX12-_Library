#pragma once
#include "Primitive3DBatch.h"
#include "../Component/Transform.h"

// 3D基礎図形の意味付けと、Primitive3DBatchへの登録を管理する
class Primitive3DSystem
{
public:
	// 内部のBatchとGPUリソースを初期化する
	bool Setup(ID3D12RootSignature* _rootSignature, ID3D12PipelineState* _fillPipelineState, ID3D12PipelineState* _wirePipelineState, ID3D12PipelineState* _debugLinePipelineState);

	// Batchが所有するGPUリソースを解放する
	void Shutdown();

	// フレーム開始時に登録済み図形をリセットする
	void Reset();

	// 登録された図形をまとめて描画する
	bool Flush(const Mat4x4& _viewProjection);

	// 単体図形をBatchへ登録する
	bool RegisterCube(const Transform& _transform, Vector4 _color, Primitive3DDrawMode _drawMode); // 箱
	bool RegisterSphere(const Transform& _transform, Vector4 _color, Primitive3DDrawMode _drawMode); // 球
	bool RegisterCylinder(const Transform& _transform, Vector4 _color, Primitive3DDrawMode _drawMode); // 円柱
	bool RegisterCapsule(Vector3 _start, Vector3 _end, float _radius, Vector4 _color, Primitive3DDrawMode _drawMode); // Capsuleを半球・円柱・半球へ分解して登録する

	
private:
	// Transformと色から、GPUへ渡せる個体データを作る
	static Primitive3DInstanceData MakeInstanceData(const Transform& _transform, Vector4 _color);

private:
	Primitive3DBatch batch{};
};
