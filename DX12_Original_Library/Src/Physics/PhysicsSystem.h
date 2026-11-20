#pragma once
#include <vector>
#include <stack>
#include <span>
#include <cstdint>
#include "PhysicsInternalType.h"
#include "../Collision/CollisionSystem.h"
#include "../Core/Handle/BodyHandle.h"

// Bodyの台帳を持ち、物理のシミュレーションを進める
class PhysicsSystem
{
public:
	PhysicsSystem() = default;
	~PhysicsSystem() = default;

	PhysicsSystem(const PhysicsSystem& _other) = delete;
	PhysicsSystem& operator=(const PhysicsSystem& _other) = delete;

	// 台帳の枠を確保する
	void Setup();
	// 終了処理
	void Shutdown();

	// 設定データからBodyを作成する
	BodyHandle CreateBody(const BodyDesc& _desc);
	// 指定ハンドルの削除
	bool DestroyBody(BodyHandle _handle);

	// 固定の刻み一回分だけ進める(重力->速度->位置)
	void Step(float _deltaTime);

	// 取得(失敗時は初期値を返す)
	Vector3 GetPosition(BodyHandle _handle);
	Quaternion GetRotation(BodyHandle _handle);
	Vector3 GetVelocity(BodyHandle _handle);
	BodyState GetState(BodyHandle _handle);

	// 設定
	bool SetPosition(BodyHandle _handle, Vector3 _position);
	bool SetRotation(BodyHandle _handle, Quaternion _rotation);
	bool SetVelocity(BodyHandle _handle, Vector3 _velocity);

	// 直近のStepで見つかった接触の一覧
	std::span<const BodyToBodyResult> GetContacts() const;

private:
	// ハンドルを検証してBodyを一つ返す
	BodyData* Lookup(BodyHandle _handle);

	// Body2つの形をワールドへ直し表から関数を選んで接触を計算する
	bool ComputeBodyContact(const BodyData& _bodyA, const BodyData& _bodyB, ShapeToShapeResult& _result);

	// 全ての組を調べて接触の一覧を作り直す
	void UpdateContacts();

private:
	std::vector<BodySlot> slots{}; // bodyの台帳
	std::stack<int> freeList{}; // 再利用できる番号

	std::vector<BodyToBodyResult> contacts{}; // 直近のStepの接触の一覧
	CollisionSystem collisionSystem{}; // 形どうしの接触を計算する

};
