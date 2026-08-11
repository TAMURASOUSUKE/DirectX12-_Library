#include <cmath>
#include "../Debug/DebugLogs.h"
#include "CameraSystem.h"

bool CameraSystem::Setup(const Camera& _camera, float _aspectRatio)
{
	// 初期のカメラ設定
	currentCamera = _camera;
	aspectRatio = _aspectRatio;
	return RecalculateMatrix();
}

bool CameraSystem::SetCamera(const Camera& _camera)
{
	// 検証成功前に前のカメラを壊さないように一時保存
	const Camera prevCamera{ currentCamera };
	currentCamera = _camera;
	if (!RecalculateMatrix())
	{
		DEBUG_LOG_WARNING("カメラを設定できませんでした設定前のカメラを適用します\n");
		currentCamera = prevCamera;
		RecalculateMatrix();
		return false;
	}
	return true;
}

bool CameraSystem::SetAspectRatio(float _aspectRatio)
{
	if (!std::isfinite(_aspectRatio) ||  _aspectRatio <= Math::EPSILON)
	{
		DEBUG_LOG_ERROR("Cameraのアスペクト比が不正です AspectRatio : {}\n", _aspectRatio);
		return false;
	}
	const float prevAspectRatio{ aspectRatio };
	aspectRatio = _aspectRatio;
	if (!RecalculateMatrix())
	{
		DEBUG_LOG_WARNING("アスペクト比を設定できませんでした設定前のアスペクト比を適用します\n");
		aspectRatio = prevAspectRatio;
		RecalculateMatrix();
		return false;
	}
	return true;
}

bool CameraSystem::RecalculateMatrix()
{
	// 視野角は0度より大きく180度より小さい必要がある
	if (!std::isfinite(currentCamera.fieldOfViewY) || currentCamera.fieldOfViewY <= Math::EPSILON || currentCamera.fieldOfViewY >= Math::PI)
	{
		DEBUG_LOG_ERROR("Cameraの視野角が不正です FieldOfViewY : {}\n", currentCamera.fieldOfViewY);
		return false;
	}
	// 有限値、0チェック、nearとfarが逆転していないか
	if (!std::isfinite(currentCamera.nearClip) || !std::isfinite(currentCamera.farClip) || currentCamera.nearClip <= Math::EPSILON || currentCamera.nearClip >= currentCamera.farClip)
	{
		DEBUG_LOG_ERROR("CameraのClip距離が不正です Near値 : {} Far値 : {}", currentCamera.nearClip, currentCamera.farClip);
		return false;
	}
	// アスペクト比値を有限値か、0チェック
	if (!std::isfinite(aspectRatio) || aspectRatio <= Math::EPSILON)
	{
		DEBUG_LOG_ERROR("Cameraのアスペクト比が不正です AspectRatio : {}", aspectRatio);
		return false;
	}

	Quaternion cameraRotation{ currentCamera.transform.GetRotation() };
	if (cameraRotation.LengthSquared() <= Math::EPSILON * Math::EPSILON)
	{
		DEBUG_LOG_ERROR("Cameraに無効なQuaternion値が設定されています\n");
		return false;
	}
	const Quaternion normalizedRotation{ Quaternion::Normalized(cameraRotation) };
	const Vector3 position{ currentCamera.transform.GetPosition() };

	// Cameraのローカル前と上をQuaternionでワールド空間へ回転させる
	const Vector3 forward{ normalizedRotation.RotateVector(Vector3::Forward) };
	const Vector3 up{ normalizedRotation.RotateVector(Vector3::Up) };

	viewMatrix = Mat4x4::MakeLookAt(position, position + forward, up);
	projectionMatrix = Mat4x4::MakePerspective(currentCamera.fieldOfViewY, aspectRatio, currentCamera.nearClip, currentCamera.farClip);

	// 行ベクトル規約のライブラリなのでView * Projectionの順
	viewProjectionMatrix = viewMatrix * projectionMatrix;
	return true;
}
