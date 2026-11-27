#pragma once
#include <vector>
#include <stack>
#include <span>
#include <cstdint>
#include "PhysicsInternalType.h"
#include "../Collision/CollisionSystem.h"
#include "../Core/Handle/BodyHandle.h"
#include "../Core/Handle/TriangleMeshHandle.h"
#include "../Core/Handle/HeightFieldHandle.h"

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

	// 頂点と番号の並びから三角形の集まりを登録する(番号は3つで三角形1枚)
	TriangleMeshHandle CreateTriangleMesh(std::span<const Vector3> _vertices, std::span<const std::uint32_t> _indices);
	// 三角形の集まりを削除する(Bodyが使っている間は断る)
	bool DestroyTriangleMesh(TriangleMeshHandle _handle);

	// 高さの並びから高さの格子を登録する(点の数はXとZそれぞれ2以上 高さは heights[z * countX + x] の順)
	HeightFieldHandle CreateHeightField(int _countX, int _countZ, float _cellSize, std::span<const float> _heights);
	// 高さの格子を削除する(Bodyが使っている間は断る)
	bool DestroyHeightField(HeightFieldHandle _handle);

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

	// Raycastを行い結果を引数で受け取る
	bool Raycast(Vector3 _origin, Vector3 _direction, float _maxDistance, RaycastResult& _result, std::uint32_t _collisionMask, BodyHandle _ignoreBody);

private:
	// ハンドルを検証してBodyを一つ返す
	BodyData* Lookup(BodyHandle _handle);
	// ハンドルを検証して三角形の集まりを一つ返す
	TriangleMeshData* Lookup(TriangleMeshHandle _handle);
	// ハンドルを検証して高さの格子を一つ返す
	HeightFieldData* Lookup(HeightFieldHandle _handle);

	// Body2つの形をワールドへ直し表から関数を選んで接触を計算する
	bool ComputeBodyContact(const BodyData& _bodyA, const BodyData& _bodyB, ShapeToShapeResult& _result);

	// 全ての組を調べて接触の一覧を作り直す
	void UpdateContacts();

	// 片方が三角形の集まりの組を三角形ごとに調べて接触を足す
	void AddTriangleMeshContacts(int _indexA, int _indexB);

	// 片方が高さの格子の組を 形が重なる升の三角形ごとに調べて接触を足す
	void AddHeightFieldContacts(int _indexA, int _indexB);

	// 接触を一つ一覧へ足す
	void PushContact(int _indexA, int _indexB, const ShapeToShapeResult& _result);

	// 接触している組の速度を直す
	void SolveVelocities();

	// 残っためり込みを位置で戻す
	void CorrectPositions();

private:
	std::vector<BodySlot> slots{}; // bodyの台帳
	std::stack<int> freeList{}; // 再利用できる番号

	std::vector<TriangleMeshSlot> triangleMeshSlots{}; // 三角形の集まりの台帳
	std::stack<int> triangleMeshFreeList{}; // 再利用できる三角形の集まりの番号

	std::vector<HeightFieldSlot> heightFieldSlots{}; // 高さの格子の台帳
	std::stack<int> heightFieldFreeList{}; // 再利用できる高さの格子の番号

	std::vector<BodyToBodyResult> contacts{}; // 直近のStepの接触の一覧
	CollisionSystem collisionSystem{}; // 形どうしの接触を計算する

	std::vector<ContactSolveData> contactSolveData{}; // contactsと同じ並びの解決用データ

};
