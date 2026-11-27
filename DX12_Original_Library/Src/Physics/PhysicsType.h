#pragma once
#include <cstdint>
#include <variant>
#include "../Collision/Collider.h"
#include "../Collision/CollisionType.h"
#include "../Core/Handle/BodyHandle.h"
#include "../Core/Handle/TriangleMeshHandle.h"
#include "../Core/Handle/HeightFieldHandle.h"
#include "../Math/Vector/Vector3.h"
#include "../Math/Quaternion/Quaternion.h"

// 物理に関係するユーザーに公開する型などを定義する

// 物理における物の動かし方
enum  class MotionType
{
	Static, // 静的な物体 物理演算による移動や速度を持たない
	Kinematic, // 運動学的物体 物理シミュレーションでは動かないが速度を持ちDynamicな物体を押し出すことができる ユーザーが直接制御する
	Dynamic, // 動的物体 すべての物理シミュレーションの影響を受ける
};

// Bodyの今の状態をまとめて受け取るためのデータ
struct BodyState
{
	Vector3 position{ Vector3::Zero }; // 位置
	Quaternion rotation{ Quaternion::Identity }; // 回転
	Vector3 velocity{ Vector3::Zero }; // 速度
};

// 登録済みの三角形の集まりを指す形状(Static専用 中身はPhysics::CreateTriangleMeshで登録する)
struct TriangleMeshShape
{
	// 登録済みのハンドルを持っているか確認
	bool IsValid() const { return mesh.IsValid(); }

	TriangleMeshHandle mesh{}; // 登録した三角形の集まり
};

// 登録済みの高さの格子を指す形状(Static専用 中身はPhysics::CreateHeightFieldで登録する Bodyの位置が格子の角になる)
struct HeightFieldShape
{
	// 登録済みのハンドルを持っているか確認
	bool IsValid() const { return heightField.IsValid(); }

	HeightFieldHandle heightField{}; // 登録した高さの格子
};

// 形状の並び 箱=0 カプセル=1 球=2 三角形の集まり=3 高さの格子=4
using Shape = std::variant<Box, Capsule, Sphere, TriangleMeshShape, HeightFieldShape>;
// BodyHandleを作成するための設定データ(形状の中に入っている位置はDesc内で設定した値からの相対的な位置になります)
struct BodyDesc
{
	Shape shapeData{}; // 形状データ
	Vector3 position{ Vector3::Zero }; // 位置
	Quaternion rotation{ Quaternion::Identity }; // 回転
	MotionType motionType{ MotionType::Static }; // 動かし方
	float mass{ 1.0f }; // 質量(内部で逆数管理をしている)
	Vector3 velocity{ Vector3::Zero }; // 初速
	Vector3 angularVelocity{ Vector3::Zero }; // 角初速
	float friction{ 0.0f }; // 摩擦
	bool isTrigger{ false }; // トリガーか
	bool isGravityEnabled{ true }; // 重力を使うか
	std::uint32_t layer{ 1u << 0 }; // 自分のレイヤーを表す
	std::uint32_t collisionMask{ 0xFFFFFFFF }; // どのレイヤーとあたり判定を行うかを表す
	std::uint64_t userData{ 0 }; // 誰に当たったかを返す際に使うユーザーが定義する値(ID)
};

// Body同士の結果
struct BodyToBodyResult
{
	ShapeToShapeResult shapeToShapeResult{}; // 形状どうしの衝突結果
	BodyHandle bodyA{}; // 接触したもの
	BodyHandle bodyB{}; // 接触されたもの
};

// Raycastの結果一つ分の構造体
struct RaycastResult
{
	BodyHandle body{}; // 当たった相手のBody
	Vector3 worldPosition{ Vector3::Zero }; // 当たったワールド位置
	Vector3 normal{ Vector3::Zero }; // 法線
	float distance{ 0.0f }; // 距離
	std::uint64_t userData{ 0 }; // 誰に当たったかを返す際に使うユーザーが定義する値(ID)
};
