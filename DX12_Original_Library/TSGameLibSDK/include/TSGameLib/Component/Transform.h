#pragma once
#include "../Math/TSMath.h"

// TRSの機能を持ったデータ群を提供するダーティフラグを用いて動きがあった時にのみ計算するようにする
class Transform
{
public:
	// デフォルトコンストラクタ
	Transform() = default;
	// デフォルトデストラクタ
	~Transform() = default;


	// 現在位置に加算する
	void Translate(const Vector3& _delta);
	
	// 現在回転に合成する
	void Rotate(const Quaternion& _delta);

	// 位置の取得
	Vector3 GetPosition() const;
	// 回転の取得
	Quaternion GetRotation() const;
	// 拡縮の取得
	Vector3 GetScale() const;
	// ワールド行列の取得(SRTの順で合成を行います)
	Mat4x4 GetWorldMatrix() const;
	// 前方向の取得
	Vector3 GetForward() const;

	// 位置を設定
	void SetPosition(const Vector3& _postion);
	// 回転を設定する
	void SetRotation(const Quaternion& _rotation);
	// 拡縮を設定する
	void SetScale(const Vector3& _scale);

private:
	Vector3 position{Vector3::Zero}; // 位置
	Quaternion rotation{Quaternion::Identity}; // 回転
	Vector3 scale{Vector3::One}; // 拡縮

	// mutableを使ってconst操作でもキャッシュを扱えるようにする(GetWorldMatはユーザーから見るとconst操作だが内部ではキャッシュが必要なため)
	mutable Mat4x4 cachedWorldMat; // Mat4x4のキャッシュ
	mutable bool isDirty{true}; // 再計算が必要か
};