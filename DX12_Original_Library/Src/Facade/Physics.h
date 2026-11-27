#pragma once
#include <span>
#include "../Physics/PhysicsType.h"
#include "../Core/Handle/BodyHandle.h"

// ユーザーに物理機能を提供する

namespace Physics 
{
	// 設定データからBodyを作成する
	BodyHandle CreateBody(const BodyDesc& _desc);

	// 指定ハンドルの削除
	bool DestroyBody(BodyHandle _handle);

	// 頂点と番号の並びから三角形の集まりを登録する(番号は3つで三角形1枚 TriangleMeshShapeに入れてStaticのBodyで使う)
	TriangleMeshHandle CreateTriangleMesh(std::span<const Vector3> _vertices, std::span<const std::uint32_t> _indices);
	// 三角形の集まりを削除する(Bodyが使っている間は断ってfalseを返す)
	bool DestroyTriangleMesh(TriangleMeshHandle _handle);

	// 高さの並びから高さの格子を登録する(点の数はXとZそれぞれ2以上 高さは heights[z * countX + x] の順 HeightFieldShapeに入れてStaticのBodyで使う)
	HeightFieldHandle CreateHeightField(int _countX, int _countZ, float _cellSize, std::span<const float> _heights);
	// 高さの格子を削除する(Bodyが使っている間は断ってfalseを返す)
	bool DestroyHeightField(HeightFieldHandle _handle);

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

	// Raycastを行い結果を引数で受け取る
	bool Raycast(Vector3 _origin, Vector3 _direction, float _maxDistance, RaycastResult& _result, std::uint32_t _collisionMask = 0xFFFFFFFF, BodyHandle _ignoreBody = {});

	// 形と重なっているBodyを全て調べて結果を引数で受け取る
	bool Overlap(const Shape& _shape, OverlapResult& _result, std::uint32_t _collisionMask = 0xFFFFFFFF, BodyHandle _ignoreBody = {});

	// 球かカプセルを動かした時に最初に触れるBodyを調べ結果を引数で受け取る
	bool Sweep(const Shape& _shape, Vector3 _direction, float _maxDistance, RaycastResult& _result, std::uint32_t _collisionMask = 0xFFFFFFFF, BodyHandle _ignoreBody = {});

}
