#pragma once
#include <optional>
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
	bool RegisterPlane(const Transform& _transform, Vector4 _color, Primitive3DDrawMode _drawMode); // 平面板
	bool RegisterLine(Vector3 _start, Vector3 _end, Vector4 _color); // 3D線分
	bool RegisterGrid(Vector3 _center, Quaternion _rotation, UINT _halfCellCount, float _cellSize, Vector4 _color); // グリッド
	bool RegisterWorldAxisGrid(Vector3 _center, UINT _halfCellCount, float _cellSize, Vector4 _xAxisColor, Vector4 _yAxisColor, Vector4 _zAxisColor); // ワールド空間軸に沿ったグリッド
	
private:
	// Transformと色から、GPUへ渡せる個体データを作る
	static Primitive3DInstanceData MakeInstanceData(const Transform& _transform, Vector4 _color);
	// 2点からLine用の登録情報を作る 長さ0の場合は登録情報を作れないのでnulloptを返す
	static std::optional<Primitive3DRegistration> MakeLineRegistration(Vector3 _start, Vector3 _end, Vector4 _color);
	// Gridを構成するLineを一時登録配列へ追加する この関数内ではBatchへ確定登録しない
	static bool AppendGridRegistrations(std::vector<Primitive3DRegistration>& _outRegistrations, Vector3 _center, Quaternion _rotation, UINT _halfCellCount, float _cellSize, Vector4 _color);

private:
	Primitive3DBatch batch{};
};
