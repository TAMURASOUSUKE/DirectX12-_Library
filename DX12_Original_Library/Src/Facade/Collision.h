#pragma once
#include "../Collision/Collider.h"

 // 判定を行う機能を提供する名前空間
namespace Collision
{
	// 矩形と矩形の接触判定
	bool Intersect(Rect _rect01, Rect _rect02);
	// 箱と箱の接触判定
	bool Intersect(Box _cube01, Box _cube02);
	// 球と球の接触判定
	bool Intersect(Sphere _sphere01, Sphere _sphere02); 
	// 球と箱の接触判定
	bool Intersect(Sphere _sphere, Box _box); 
	// カプセルとカプセルの接触判定
	bool Intersect(Capsule _capsule01, Capsule _capsule02);
	// カプセルと箱の接触判定
	bool Intersect(Capsule _capsule, Box _box); 
	// カプセルと球の接触判定
	bool Intersect(Capsule _capsule, Sphere _sphere); 
	bool Intersect(Box _box, Sphere _sphere); // 異種形状の逆順
	bool Intersect(Box _box, Capsule _capsule);
	bool Intersect(Sphere _sphere, Capsule _capsule);

	// 矩形と矩形の衝突判定 第一引数を固定された第二引数から押し戻す量をoutで出す
	bool ComputePushOut(Rect _movable, Rect _obstacle, Vector2& _outMove);
	// 箱と箱の衝突判定 第一引数を固定された第二引数から押し戻す量をoutで出す
	bool ComputePushOut(Box _movable, Box _obstacle, Vector3& _outMove);
	// 球と球の衝突判定 第一引数を固定された第二引数から押し戻す量をoutで出す
	bool ComputePushOut(Sphere _movable, Sphere _obstacle, Vector3& _outMove);
	// 球と箱の衝突判定 第一引数を固定された第二引数から押し戻す量をoutで出す
	bool ComputePushOut(Sphere _movable, Box _obstacle, Vector3& _outMove);
	// 箱と球の衝突判定 第一引数を固定された第二引数から押し戻す量をoutで出す
	bool ComputePushOut(Box _movable, Sphere _obstacle, Vector3& _outMove);
	// カプセルとカプセルの衝突判定 第一引数を固定された第二引数から押し戻す量をoutで出す
	bool ComputePushOut(Capsule _movable, Capsule _obstacle, Vector3& _outMove);
	// カプセルと箱の衝突判定 第一引数を固定された第二引数から押し戻す量をoutで出す
	bool ComputePushOut(Capsule _movable, Box _obstacle, Vector3& _outMove);
	// 箱とカプセルの衝突判定 第一引数を固定された第二引数から押し戻す量をoutで出す
	bool ComputePushOut(Box _movable, Capsule _obstacle, Vector3& _outMove);
	// カプセルと球の衝突判定 第一引数を固定された第二引数から押し戻す量をoutで出す
	bool ComputePushOut(Capsule _movable, Sphere _obstacle, Vector3& _outMove);
	// 球とカプセルの衝突判定 第一引数を固定された第二引数から押し戻す量をoutで出す
	bool ComputePushOut(Sphere _movable, Capsule _obstacle, Vector3& _outMove);



}
