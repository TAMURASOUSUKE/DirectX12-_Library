#pragma once
#include "../Component/Camera.h"

// 現在使用するカメラと各種カメラ行列を管理する
class CameraSystem
{
public:
	// 初期カメラと描画先のアスペクト比を設定する
	bool Setup(const Camera& _camera, float _aspectRatio);

	// 現在使用するカメラを変更する
	bool SetCamera(const Camera& _camera);
	// 描画先サイズが変わった時にアスペクト比を更新する
	bool SetAspectRatio(float _aspectRatio);

	// View行列の取得
	const Mat4x4& GetViewMatrix() const { return viewMatrix; }
	// Projection行列の取得
	const Mat4x4& GetProjectionMatrix() const { return projectionMatrix; }
	// VP行列の取得
	const Mat4x4& GetViewProjectionMatrix() const { return viewProjectionMatrix; }
	// 現在設定されているカメラの現在位置の取得
	Vector3 GetCameraPosition() const { return currentCamera.transform.GetPosition(); }

private:
	// 現在のカメラ設定から行列を再計算する
	bool RecalculateMatrix();

private:
	Camera currentCamera{}; // 値保持する(カメラが破棄された時にタングリングを起こさないため)
	float aspectRatio{ 1.0f }; // アスペクト比

	// カメラが持つ各種行列
	Mat4x4 viewMatrix{ Mat4x4::Identity };
	Mat4x4 projectionMatrix{ Mat4x4::Identity };
	Mat4x4 viewProjectionMatrix{ Mat4x4::Identity };
};
