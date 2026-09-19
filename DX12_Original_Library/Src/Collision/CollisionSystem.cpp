#include <algorithm>
#include "../Math/TSMath.h"
#include "CollisionSystem.h"

bool CollisionSystem::Intersect(Rect _rect01, Rect _rect02)
{
	if (!_rect01.IsValid() || !_rect02.IsValid()) return false;

	const Vector2 min01{_rect01.GetMinPos()}; // 一つめの最小
	const Vector2 max01{ _rect01.GetMaxPos() }; // 一つめの最大
	const Vector2 min02{ _rect02.GetMinPos() }; // 二つめの最小
	const Vector2 max02{ _rect02.GetMaxPos() }; // 二つ目の最大
	// 判定を行う どれか一つでも重なっていなければ当たっていない
	if (max01.x < min02.x || min01.x > max02.x) return false;
	if (max01.y < min02.y || min01.y > max02.y) return false;
	return true;
}

bool CollisionSystem::Intersect(Box _box01, Box _box02)
{
	if (!_box01.IsValid() || !_box02.IsValid()) return false;

	// 回転がない場合はAABBを行う
	if (_box01.rotation == Quaternion::Identity && _box02.rotation == Quaternion::Identity)
	{
		const Vector3 min01{ _box01.center - _box01.halfSize }; // 一つめの最小
		const Vector3 max01{ _box01.center +_box01.halfSize }; // 一つめの最大
		const Vector3 min02{ _box02.center - _box02.halfSize }; // 二つめの最小
		const Vector3 max02{ _box02.center + _box02.halfSize }; // 二つ目の最大
		// 判定を行う どれか一つでも重なっていなければ当たっていない
		if (max01.x < min02.x || min01.x > max02.x) return false;
		if (max01.y < min02.y || min01.y > max02.y) return false;
		if (max01.z < min02.z || min01.z > max02.z) return false;
		return true;
	}

	// OBBをSATで行う
	const Vector3 dist{ Vector3::FromTo(_box01.center, _box02.center) }; // 中心間

	// 各軸をRotatevectorで回してワールド空間の方向を出す
	const Vector3 axisX01{ _box01.rotation.RotateVector({1.0f, 0.0f, 0.0f}) };
	const Vector3 axisY01{ _box01.rotation.RotateVector({0.0f, 1.0f, 0.0f}) };
	const Vector3 axisZ01{ _box01.rotation.RotateVector({0.0f, 0.0f, 1.0f}) };
	const Vector3 axisX02{ _box02.rotation.RotateVector({1.0f, 0.0f, 0.0f}) };
	const Vector3 axisY02{ _box02.rotation.RotateVector({0.0f, 1.0f, 0.0f}) };
	const Vector3 axisZ02{ _box02.rotation.RotateVector({0.0f, 0.0f, 1.0f}) };

	// 中心から面までのベクトル
	const Vector3 halfEdgeX01{ axisX01 * _box01.halfSize.x };
	const Vector3 halfEdgeY01{ axisY01 * _box01.halfSize.y };
	const Vector3 halfEdgeZ01{ axisZ01 * _box01.halfSize.z };
	const Vector3 halfEdgeX02{ axisX02 * _box02.halfSize.x };
	const Vector3 halfEdgeY02{ axisY02 * _box02.halfSize.y };
	const Vector3 halfEdgeZ02{ axisZ02 * _box02.halfSize.z };

	// 任意軸を受け取りその軸上において二つの箱が重なっているかをチェックする
	const auto overlapsOnAxis = [&dist, &halfEdgeX01, &halfEdgeY01, &halfEdgeZ01, &halfEdgeX02, &halfEdgeY02, &halfEdgeZ02](const Vector3& _n)
		{
			const float distDotN{ std::abs(Vector3::Dot(dist, _n)) }; // 分離軸候補に投影した中心間隔
			const float rA{ std::abs(Vector3::Dot(halfEdgeX01, _n)) + std::abs(Vector3::Dot(halfEdgeY01, _n)) + std::abs(Vector3::Dot(halfEdgeZ01, _n)) }; // 片方のBoxの分離軸に対する投影線分
			const float rB{ std::abs(Vector3::Dot(halfEdgeX02, _n)) + std::abs(Vector3::Dot(halfEdgeY02, _n)) + std::abs(Vector3::Dot(halfEdgeZ02, _n)) }; // 片方のBoxの分離軸に対する投影線分
			return (distDotN <= (rA + rB));
		};

	if (!overlapsOnAxis(axisX01)) return false; // Box1のローカルX軸方向の分離軸
	if (!overlapsOnAxis(axisY01)) return false; // Box1のローカルY軸方向の分離軸
	if (!overlapsOnAxis(axisZ01)) return false; // Box1のローカルZ軸方向の分離軸
	if (!overlapsOnAxis(axisX02)) return false; // Box2のローカルX軸方向の分離軸
	if (!overlapsOnAxis(axisY02)) return false; // Box2のローカルY軸方向の分離軸
	if (!overlapsOnAxis(axisZ02)) return false; // Box2のローカルZ軸方向の分離軸
	if (!overlapsOnAxis(Vector3::Cross(axisX01, axisX02))) return false; // Box1のローカルXとBox2のローカルXの2の外積による分離軸
	if (!overlapsOnAxis(Vector3::Cross(axisX01, axisY02))) return false; // Box1のローカルXとBox2のローカルYの2の外積による分離軸
	if (!overlapsOnAxis(Vector3::Cross(axisX01, axisZ02))) return false; // Box1のローカルXとBox2のローカルZの2の外積による分離軸
	if (!overlapsOnAxis(Vector3::Cross(axisY01, axisX02))) return false; // Box1のローカルYとBox2のローカルXの2の外積による分離軸
	if (!overlapsOnAxis(Vector3::Cross(axisY01, axisY02))) return false; // Box1のローカルYとBox2のローカルYの2の外積による分離軸
	if (!overlapsOnAxis(Vector3::Cross(axisY01, axisZ02))) return false; // Box1のローカルYとBox2のローカルZの2の外積による分離軸
	if (!overlapsOnAxis(Vector3::Cross(axisZ01, axisX02))) return false; // Box1のローカルZとBox2のローカルXの2の外積による分離軸
	if (!overlapsOnAxis(Vector3::Cross(axisZ01, axisY02))) return false; // Box1のローカルZとBox2のローカルYの2の外積による分離軸
	if (!overlapsOnAxis(Vector3::Cross(axisZ01, axisZ02))) return false; // Box1のローカルZとBox2のローカルZの2の外積による分離軸

	// ここまできたら当たっていると判定
	return true;
}

bool CollisionSystem::Intersect(Sphere _sphere01, Sphere _sphere02)
{
	if (!_sphere01.IsValid() || !_sphere02.IsValid()) return false; // 正しい値が入っているか確認

	const Vector3 center01{ _sphere01.center }; // 一つ目の球の中心
	const Vector3 center02{ _sphere02.center }; // 二つ目の球の中心

	const float distanceSquared{ Vector3::DistanceSquared(center01, center02) }; // 中心の距離の二乗
	const float totalRadiusSquared{ (_sphere01.radius + _sphere02.radius) * (_sphere01.radius + _sphere02.radius) }; // 半径の合計の二乗

	if (distanceSquared > totalRadiusSquared) return false; // 二つの球の半径の合計値より距離が離れていればfalse

	return  true;
}

bool CollisionSystem::Intersect(Sphere _sphere, Box  _box)
{
	if (!_sphere.IsValid() || !_box.IsValid()) return false; // 不正な値ならfalse

	const Vector3 distance{ Vector3::FromTo(_box.center, _sphere.center) }; // 中心同士のベクトル(差分)
	const Quaternion reverseRot{ Vector4{-_box.rotation.x, -_box.rotation.y, -_box.rotation.z, _box.rotation.w} }; // 箱の逆回転
	const Vector3 localCenter{reverseRot.RotateVector(distance)}; // 箱基準の球の中心座標

	// 最近点を求める
	const Vector3 nearestPosition{ std::clamp(localCenter.x, -_box.halfSize.x, _box.halfSize.x),
															  std::clamp(localCenter.y, -_box.halfSize.y, _box.halfSize.y),
															  std::clamp(localCenter.z, -_box.halfSize.z, _box.halfSize.z) };

	return Vector3::DistanceSquared(nearestPosition, localCenter) <= _sphere.radius * _sphere.radius;
}

