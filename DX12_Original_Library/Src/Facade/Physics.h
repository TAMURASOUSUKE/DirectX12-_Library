#pragma once
#include "../Physics/PhysicsType.h"
#include "../Core/Handle/BodyHandle.h"

// ユーザーに物理機能を提供する

namespace Physics 
{
	// 設定データからBodyを作成する
	BodyHandle CreateBody(const BodyDesc& _desc);

	// 指定ハンドルの削除
	bool DestroyBody(BodyHandle _handle);

	// 固定更新の中で1回呼ぶ(Time::FixedDeltaTimeを渡す)
	void Step(float _deltaTime);

	// 位置を取得する(失敗時は初期値)
	Vector3 GetPosition(BodyHandle _handle);
	// 回転を取得する(失敗時は初期値)
	Quaternion GetRotation(BodyHandle _handle);
	// 速度を取得する(失敗時は初期値)
	Vector3 GetVelocity(BodyHandle _handle);
	// 位置・回転・速度をまとめて取得する
	BodyState GetState(BodyHandle _handle);

	// 位置を設定する
	bool SetPosition(BodyHandle _handle, Vector3 _position);
	// 回転を設定する
	bool SetRotation(BodyHandle _handle, Quaternion _rotation);
	// 速度を設定する
	bool SetVelocity(BodyHandle _handle, Vector3 _velocity);

	// 直近のStepで見つかった接触の一覧を取得する(次のStepまで有効)
	std::span<const BodyToBodyResult> GetContacts();
}
