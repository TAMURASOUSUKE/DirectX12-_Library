#pragma once
#include "../../Src/Facade/TSLib.h"
// 各オブジェクトの基底

// オブジェクトの種類
enum class ObjectType
{
	Background,
	Player,
	Car
};

class ObjectBase
{
public:
	virtual ~ObjectBase() = default;

	// 更新と描画
	virtual void Update() = 0;
	virtual void Draw() = 0;

	// オブジェクトの種類を取得する
	virtual ObjectType GetObjectType() const = 0;

	// 衝突した際の処理
	virtual void OnCollision(ObjectBase& _other) {}

	// 生きているか返す
	bool GetIsActive() const { return isActive; }

	// 現在の位置とサイズから当たり判定を作る
	Rect GetCollisionRect() const
	{
		return Rect{position, size};
	}

protected:
	ObjectBase() = default;
	// ハンドル、位置、サイズ、回転
	ObjectBase(TexHandle _handle, const Vector2& _position, const Vector2  _size, float _rotate) :handle{_handle}, position { _position }, size{ _size }, rotate{ _rotate } {}

protected:
	TexHandle handle{}; // 画像ハンドル
	Vector2 position{Vector2::Zero}; // 位置
	Vector2 size{Vector2::One}; // 大きさ
	float rotate{ 0.0f }; // 回転
	bool isActive{ true }; // 生きているか
};