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
		DEBUG_LOG_ERROR("RegisterCapsule3Dに不正な半径が渡されました Radius : {}\n", _radius);
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

	// すでにLengthが求まっているのでNormalizeを呼んで計算を重複させない
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

	// 3つを一つのグループとして登録する
	const std::array<Primitive3DRegistration, 3> registrations
	{
		Primitive3DRegistration
		{
			Primitive3DMeshID::Cylinder,
			MakeInstanceData(cylinder, _color),
			_drawMode
		},
			Primitive3DRegistration
		{
			Primitive3DMeshID::Hemisphere,
			MakeInstanceData(endCap, _color),
			_drawMode
		},
			Primitive3DRegistration
		{
			Primitive3DMeshID::Hemisphere,
			MakeInstanceData(startCap, _color),
			_drawMode
		},
	};
	return batch.RegisterGroup(registrations);
}

bool Primitive3DSystem::RegisterPlane(const Transform& _transform, Vector4 _color, Primitive3DDrawMode _drawMode)
{
	return batch.Register(Primitive3DMeshID::Plane, MakeInstanceData(_transform, _color), _drawMode);
}

bool Primitive3DSystem::RegisterLine(Vector3 _start, Vector3 _end, Vector4 _color)
{
	const auto registration{ MakeLineRegistration(_start, _end, _color) };
	if (!registration) return true; // 長さ0は異常終了ではなく何もかかない正常な処理とする

	// 必ずLINE_LIST描画
	return batch.Register(registration->meshID, registration->instance, registration->drawMode);
}

bool Primitive3DSystem::RegisterGrid(Vector3 _center, Quaternion _rotation, UINT _halfCellCount, float _cellSize, Vector4 _color)
{
	// 一時配列
	std::vector<Primitive3DRegistration> registrations{};
	// Gridを作成する
	if (!AppendGridRegistrations(registrations, _center, _rotation, _halfCellCount, _cellSize, _color)) return false;
	// 完成後に一度だけ登録確定
	return batch.RegisterGroup(registrations);
}

bool Primitive3DSystem::RegisterWorldAxisGrid(Vector3 _center, UINT _halfCellCount, float _cellSize, Vector4 _xAxisColor, Vector4 _yAxisColor, Vector4 _zAxisColor)
{
	// 一時配列
	std::vector<Primitive3DRegistration> registrations{};
	// 作成に失敗しても一時配列が破棄されるのでCommit前の状態に戻る
	if (!AppendGridRegistrations(registrations, _center, Quaternion::Identity, _halfCellCount, _cellSize, _xAxisColor)) return false; // XZ平面
	if (!AppendGridRegistrations(registrations, _center, Quaternion::FromEuler({90.0f * Math::DEG_TO_RAD, 0.0f, 0.0f}), _halfCellCount, _cellSize, _yAxisColor)) return false; // XY平面
	if (!AppendGridRegistrations(registrations, _center, Quaternion::FromEuler({ 0.0f, 0.0f, 90.0f * Math::DEG_TO_RAD }), _halfCellCount, _cellSize, _zAxisColor)) return false; // YZ平面
	// 完成後に一度だけ登録確定
	return batch.RegisterGroup(registrations);
}

Primitive3DInstanceData Primitive3DSystem::MakeInstanceData(const Transform& _transform, Vector4 _color)
{
	Primitive3DInstanceData instance{};
	instance.world = _transform.GetWorldMatrix();

	const Vector3 scale{ _transform.GetScale() };
	const Quaternion rotation{ _transform.GetRotation() };

	// 非均一スケールでも法線と面の直角関係を維持するために各軸のスケールの逆数を使う
	const Vector3 inverseScale{ 1.0f / scale.x, 1.0f / scale.y, 1.0f / scale.z };

	// SRTに対応する法線行列を作る
	instance.worldInverseTranspose = Mat4x4::MakeScaling(inverseScale) * rotation.ToMat4x4();
	instance.color = _color;

	return instance;
}

std::optional<Primitive3DRegistration> Primitive3DSystem::MakeLineRegistration(Vector3 _start, Vector3 _end, Vector4 _color)
{
	const Vector3 axis{ _end - _start };
	const float length{ axis.Length() };

	// 線分が0なら何もせず正常と判断する
	if (length <= Math::EPSILON) return std::nullopt;

	// すでにLengthが求まっているのでNormalizeを呼んで計算を重複させない
	const Vector3 direction{ axis / length };
	const Vector3 center{ (_start + _end) * 0.5f };

	Transform line{};
	line.SetPosition(center);
	// 単位Lineの基準方向 +Xを始点から終点の方へ向かわせる
	line.SetRotation(Quaternion::FromToRotation(Vector3::Right, direction));
	line.SetScale({ length, 1.0f, 1.0f });

	return Primitive3DRegistration{ Primitive3DMeshID::Line, MakeInstanceData(line, _color), Primitive3DDrawMode::DebugLine };
}

bool Primitive3DSystem::AppendGridRegistrations(std::vector<Primitive3DRegistration>& _outRegistrations, Vector3 _center, Quaternion _rotation, UINT _halfCellCount, float _cellSize, Vector4 _color)
{
	if (_halfCellCount == 0)
	{
		DEBUG_LOG_ERROR("Gridの片側マス数には1以上を設定してください\n");
		return false;
	}
	if (_cellSize <= Math::EPSILON)
	{
		DEBUG_LOG_ERROR("Gridのマスサイズに不正な値が渡されました CellSize : {}\n", _cellSize);
		return false;
	}

	const std::size_t lineCountPerAxis{ static_cast<std::size_t>(_halfCellCount) * 2 + 1 };
	const std::size_t additionalLineCount{ lineCountPerAxis * 2 };

	// 加算式にするとオーバーフローする可能性があるため残り容量と追加数を比較する
	if (_outRegistrations.size() > MAX_PRIMITIVE_3D_INSTANCE_COUNT)
	{
		DEBUG_LOG_ERROR("Grid一時登録配列が不正なサイズです\n");
		return false;
	}
	const std::size_t remainingCapacity{ MAX_PRIMITIVE_3D_INSTANCE_COUNT - _outRegistrations.size() }; // 残り容量
	if (additionalLineCount > remainingCapacity)
	{
		DEBUG_LOG_ERROR("Gridの構成要素がPrimitive3D上限を超えます Current : {} Add : {} Max : {}\n", _outRegistrations.size(), additionalLineCount, MAX_PRIMITIVE_3D_INSTANCE_COUNT);
		return false;
	}

	// このGridを追加する前のサイズを覚えておく
	const std::size_t originalSize{ _outRegistrations.size() };
	_outRegistrations.reserve(originalSize + additionalLineCount);

	const float extent{ static_cast<float>(_halfCellCount) * _cellSize };
	const int halfCellCount{ static_cast<int>(_halfCellCount) };

	const auto ToWorld = [&_center, &_rotation](const Vector3& _localPosition)
		{
			return _center + _rotation.RotateVector(_localPosition);
		};

	for (int i = -halfCellCount; i <= halfCellCount; i++)
	{
		const float offset{ static_cast<float>(i) * _cellSize };
		const auto xLine{ MakeLineRegistration(ToWorld({ -extent, 0.0f, offset }), ToWorld({  extent, 0.0f, offset }), _color) };
		const auto zLine{ MakeLineRegistration(ToWorld({ offset, 0.0f, -extent }), ToWorld({ offset, 0.0f,  extent }), _color) };

		if (!xLine || !zLine)
		{
			// このGridを作る前の状態へ戻す
			_outRegistrations.resize(originalSize);
			DEBUG_LOG_ERROR("Gridを構成するLineの生成に失敗しました\n");
			return false;
		}
		_outRegistrations.push_back(*xLine);
		_outRegistrations.push_back(*zLine);
	}
	return true;
}
