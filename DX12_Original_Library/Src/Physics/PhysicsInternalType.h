#pragma once
#include "../Math/Vector/Vector3.h"
#include "PhysicsType.h"
#include "../Math/Quaternion/Quaternion.h"

// ユーザーに公開しない物理の型などを定義する

// Body一個分のデータ
struct BodyData
{
	Shape shape{}; // 形状データ(中の位置はpositionからの相対)
	Vector3 position{ Vector3::Zero }; // 今の位置
	Quaternion rotation{ Quaternion::Identity }; // 今の回転
	Vector3 velocity{ Vector3::Zero }; // 現在の速度
	Vector3 angularVelocity{ Vector3::Zero }; // 現在の角速度
	float massInverse{ 0.0f }; // 質量の逆数(StaticとKinematicは0)
	float friction{ 0.0f }; // 摩擦
	MotionType motionType{ MotionType::Static }; // 動かし方
	bool isTrigger{ false }; // トリガーか
	bool isGravityEnabled{ true }; // 重力を使うか
	std::uint32_t layer{ 1u << 0 }; // 自分のレイヤー
	std::uint32_t collisionMask{ 0xFFFFFFFF }; // 判定を行う相手のレイヤー
	std::uint64_t userData{ 0 }; // ユーザーが定義する値
};

// Body一つ分のスロット
struct BodySlot
{
	BodyData data{}; // 実データ
	std::uint32_t generation{ 0 }; // 世代
	bool isAlive{ false }; // 生存フラグ
};

// 接触1つ分の解決用データ(contactsと同じ並びで持つ)
struct ContactSolveData
{
	int indexA{ 0 }; // slotsの中のAの番号
	int indexB{ 0 }; // slotsの中のBの番号
	Vector3 tangent1{ Vector3::Zero }; // 法線に垂直な向き1(摩擦をかける向き)
	Vector3 tangent2{ Vector3::Zero }; // 法線に垂直な向き2
	float normalImpulse{ 0.0f }; // 法線の向きに与えた力積の合計
	float tangentImpulse1{ 0.0f }; // 向き1に与えた摩擦の力積の合計
	float tangentImpulse2{ 0.0f }; // 向き2に与えた摩擦の力積の合計
	float inverseMassSum{ 0.0f }; // 2つの質量の逆数の合計
	float friction{ 0.0f }; // この組の摩擦の強さ
	bool isSolved{ false }; // 押し戻す組か(トリガーはfalse)
};

