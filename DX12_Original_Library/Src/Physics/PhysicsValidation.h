#pragma once
#include "../Math/Quaternion/Quaternion.h"
#include "../Math/Vector/Vector3.h"
#include "PhysicsInternalType.h"
#include "PhysicsType.h"

// 入力の確認を行うヘルパーを置くファイル
namespace PhysicsValidation
{
	// 有限値かをVector3で直接調べるヘルパー
	bool IsFinite(const Vector3& _value);

	// 有限で長さ1の回転か
	bool IsValidRotation(const Quaternion& _rotation);

	// 作成に使える設定か
	bool IsValidDesc(const BodyDesc& _desc);

	// 設定から内部データを作る
	BodyData MakeBodyData(const BodyDesc& _desc);
}
