#pragma once
#include <cstddef>
#include <variant>
#include "../Collision/Collider.h"
#include "../Math/Vector/Vector3.h"
#include "../Math/Quaternion/Quaternion.h"

// 物理における物の動かし方
enum  class MotionType
{
	Static, // 静的な物体 物理演算による移動や速度を持たない
	Kinematic, // 運動学的物体 物理シミュレーションでは動かないが速度を持ちDynamicな物体を押し出すことができる ユーザーが直接制御する
	Dynamic, // 動的物体 すべての物理シミュレーションの影響を受ける
};

using Shape = std::variant<Box, Capsule, Sphere>;
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
