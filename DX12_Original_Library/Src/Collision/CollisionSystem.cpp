#include <algorithm>
#include "../Math/TSMath.h"
#include "CollisionSystem.h"

bool CollisionSystem::Intersect(Rect _rect01, Rect _rect02)
{
	const Vector2 min01{_rect01.GetMinPos()}; // 一つめの最小
	const Vector2 max01{ _rect01.GetMaxPos() }; // 一つめの最大
	const Vector2 min02{ _rect02.GetMinPos() }; // 二つめの最小
	const Vector2 max02{ _rect02.GetMaxPos() }; // 二つ目の最大
	// 判定を行う どれか一つでも重なっていなければ当たっていない
	if (max01.x < min02.x || min01.x > max02.x) return false;
	if (max01.y < min02.y || min01.y > max02.y) return false;
	return true;
}

bool CollisionSystem::Intersect(Box _cube01, Box _cube02)
{
	const Vector3 min01{ _cube01.minPosition }; // 一つめの最小
	const Vector3 max01{ _cube01.maxPosition }; // 一つめの最大
	const Vector3 min02{ _cube02.minPosition }; // 二つめの最小
	const Vector3 max02{ _cube02.maxPosition }; // 二つ目の最大
	// 判定を行う どれか一つでも重なっていなければ当たっていない
	if (max01.x < min02.x || min01.x > max02.x) return false;
	if (max01.y < min02.y || min01.y > max02.y) return false;
	if (max01.z < min02.z || min01.z > max02.z) return false;
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

bool Intersect(Sphere _sphere, Box  _box)
{
	if (!_sphere.IsValid()) return false; // 不正な値ならfalse

	const Vector3 sphereCenter{ _sphere.center }; // 球の中心
	const Vector3 boxMin{ _box.minPosition }; // 箱の最小座標
	const Vector3 boxMax{ _box.maxPosition }; // 箱の最大座標

	// 級の中心から箱の最近点を求めている
	const Vector3 nearestPos{ std::clamp(sphereCenter.x, boxMin.x, boxMax.x),
													 std::clamp(sphereCenter.y, boxMin.y, boxMax.y),
													 std::clamp(sphereCenter.z, boxMin.z, boxMax.z) };

	const float radiusSquared{ _sphere.radius * _sphere.radius }; // 半径の二乗
	const float distanceSquared{ Vector3::DistanceSquared(nearestPos, sphereCenter) }; // 最近点と級の中心の二乗

	


	return true;
}

