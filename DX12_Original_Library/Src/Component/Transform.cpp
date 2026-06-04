#include <iostream>
#include <windows.h>
#include "Transform.h"

// 現在位置に加算する
void Transform::Translate(const Vector3& _delta)
{
	isDirty = true;
	position += _delta;
}

void Transform::Rotate(const Quaternion& _delta)
{
	isDirty = true;
	rotation = rotation * _delta;
}

// 位置の取得
Vector3 Transform::GetPosition() const
{
	return position;
}

// 回転の取得
Quaternion Transform::GetRotation() const
{
	return rotation;
}

// 拡縮の取得
Vector3 Transform::GetScale() const
{
	return scale;
}

// ワールド行列の取得
Mat4x4 Transform::GetWorldMatrix() const
{
	// 変更があった時だけ計算
	if (isDirty)
	{
		Mat4x4 S{ Mat4x4::MakeScaling(scale) }; // Scaleを行列に変換
		Mat4x4 R{ rotation.ToMat4x4() }; // Rotaionを行列に変換
		
		char buf[512]{};
		sprintf_s(buf,
			"World\n%f %f %f %f\n%f %f %f %f\n%f %f %f %f\n%f %f %f %f\n",
			cachedWorldMat.rows[0].x, cachedWorldMat.rows[0].y, cachedWorldMat.rows[0].z, cachedWorldMat.rows[0].w,
			cachedWorldMat.rows[1].x, cachedWorldMat.rows[1].y, cachedWorldMat.rows[1].z, cachedWorldMat.rows[1].w,
			cachedWorldMat.rows[2].x, cachedWorldMat.rows[2].y, cachedWorldMat.rows[2].z, cachedWorldMat.rows[2].w,
			cachedWorldMat.rows[3].x, cachedWorldMat.rows[3].y, cachedWorldMat.rows[3].z, cachedWorldMat.rows[3].w);
		OutputDebugStringA(buf);

		Mat4x4 T{ Mat4x4::MakeTranslation(position) }; // positionをTranslation行列に変換

		cachedWorldMat = S * R * T;
		isDirty = false;
	}
	return cachedWorldMat;
}
// 前方向の取得
Vector3 Transform::GetForward() const
{
	return rotation.RotateVector(Vector3::Forward);
}

//　位置の設定
void Transform::SetPosition(const Vector3& _pos)
{
	isDirty = true; // 変更が起こったのでフラグを立てる
	position = _pos;
}

// 回転の設定
void Transform::SetRotation(const Quaternion& _rotation)
{
	isDirty = true; // 変更が起こったのでフラグを立てる
	rotation = _rotation;
}

// 拡縮の設定
void Transform::SetScale(const Vector3& _scale)
{
	isDirty = true; // 変更が起こったのでフラグを立てる
	scale = _scale;
}