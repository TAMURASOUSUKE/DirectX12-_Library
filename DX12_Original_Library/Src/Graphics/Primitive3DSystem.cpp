#include "../Debug/DebugLogs.h"
#include "Primitive3DSystem.h"


bool Primitive3DSystem::Setup(ID3D12RootSignature* _rootSignature, ID3D12PipelineState* _fillPipelineState, ID3D12PipelineState* _wirePipelineState, ID3D12PipelineState* _debugLinePipelineState)
{
	return batch.Setup(_rootSignature, _fillPipelineState, _wirePipelineState, _debugLinePipelineState);
}

void Primitive3DSystem::Shutdown()
{
	batch.Shutdown();
}

void Primitive3DSystem::Reset()
{
	batch.Reset();
}

bool Primitive3DSystem::Flush(const Mat4x4& _viewProjection)
{
	return batch.Flush(_viewProjection);
}

bool Primitive3DSystem::RegisterCube(const Transform& _transform, Vector4 _color, Primitive3DDrawMode _drawMode)
{
	return batch.Register(Primitive3DMeshID::Cube, MakeInstanceData(_transform, _color), _drawMode);
}

bool Primitive3DSystem::RegisterSphere(const Transform& _transform, Vector4 _color, Primitive3DDrawMode _drawMode)
{
	return batch.Register(Primitive3DMeshID::Sphere, MakeInstanceData(_transform, _color), _drawMode);
}

bool Primitive3DSystem::RegisterCylinder(const Transform& _transform, Vector4 _color, Primitive3DDrawMode _drawMode)
{
	return batch.Register(Primitive3DMeshID::Cylinder, MakeInstanceData(_transform, _color), _drawMode);
}

bool Primitive3DSystem::RegisterCapsule(Vector3 _start, Vector3 _end, float _radius, Vector4 _color, Primitive3DDrawMode _drawMode)
{
	if (_radius <= Math::EPSILON)
	{
		DEBUG_LOG_ERROR("DrawCapsule3Dに不正な半径が渡されました Radius : {}\n", _radius);
		return false;
	}

	const Vector3 axis{ _end - _start };
	const float length{ axis.Length() };
	const float diameter{ _radius * 2.0f };

	// 線分が0ならCapsuleはSphereとする
	if (length <= Math::EPSILON)
	{
		Transform sphere{};
		sphere.SetPosition(_start);
		sphere.SetScale({ diameter, diameter, diameter });
		return batch.Register(Primitive3DMeshID::Sphere, MakeInstanceData(sphere, _color), _drawMode);
	}

	// すでにLengthがも止まっているのでNormalizeを呼んで計算を重複させない
	const Vector3 direction{ axis / length };
	const Vector3 center{ (_start + _end) * 0.5f };

	// 中央のCylinder
	Transform cylinder{};
	cylinder.SetPosition(center);
	cylinder.SetRotation(Quaternion::FromToRotation(Vector3::Up, direction));
	cylinder.SetScale({ diameter, length, diameter });

	// endのHemisphere
	Transform endCap{};
	endCap.SetPosition(_end);
	endCap.SetRotation(Quaternion::FromToRotation(Vector3::Up, direction));
	endCap.SetScale({ diameter, diameter, diameter });

	// start側のHemisphere
	Transform startCap{};
	startCap.SetPosition(_start);
	startCap.SetRotation(Quaternion::FromToRotation(Vector3::Up, -direction));
	startCap.SetScale({ diameter, diameter, diameter });

	// 各登録
	if (!batch.Register(Primitive3DMeshID::Cylinder, MakeInstanceData(cylinder, _color), _drawMode) ||
		!batch.Register(Primitive3DMeshID::Hemisphere, MakeInstanceData(endCap, _color), _drawMode) ||
		!batch.Register(Primitive3DMeshID::Hemisphere, MakeInstanceData(startCap, _color), _drawMode))
	{
		return false;
	}
	return true;
}

Primitive3DInstanceData Primitive3DSystem::MakeInstanceData(const Transform& _transform, Vector4 _color)
{
	Primitive3DInstanceData instance{};
	instance.world = _transform.GetWorldMatrix();

	// ライト接続までは暫定
	instance.worldInverseTranspose = Mat4x4::Identity;
	instance.color = _color;

	return instance;
}
