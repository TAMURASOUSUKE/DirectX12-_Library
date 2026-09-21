#include <algorithm>
#include <array>
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
	const Vector3 dir{ Vector3::FromTo(_box01.center, _box02.center) }; // 中心間

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
	const auto overlapsOnAxis = [&dir, &halfEdgeX01, &halfEdgeY01, &halfEdgeZ01, &halfEdgeX02, &halfEdgeY02, &halfEdgeZ02](const Vector3& _n)
		{
			const float distDotN{ std::abs(Vector3::Dot(dir, _n)) }; // 分離軸候補に投影した中心間隔
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

	const Vector3 dir{ Vector3::FromTo(_box.center, _sphere.center) }; // 中心同士のベクトル(差分)
	const Quaternion reverseRot{ Vector4{-_box.rotation.x, -_box.rotation.y, -_box.rotation.z, _box.rotation.w} }; // 箱の逆回転
	const Vector3 localCenter{reverseRot.RotateVector(dir)}; // 箱基準の球の中心座標

	// 最近点を求める
	const Vector3 nearestPosition{ std::clamp(localCenter.x, -_box.halfSize.x, _box.halfSize.x),
															  std::clamp(localCenter.y, -_box.halfSize.y, _box.halfSize.y),
															  std::clamp(localCenter.z, -_box.halfSize.z, _box.halfSize.z) };

	return Vector3::DistanceSquared(nearestPosition, localCenter) <= _sphere.radius * _sphere.radius;
}

bool CollisionSystem::Intersect(Capsule _capsule01, Capsule _capsule02)
{
	if (!_capsule01.IsValid() || !_capsule02.IsValid()) return false;
	const Vector3 A{ _capsule01.startPos };
	const Vector3 B{ _capsule01.endPos };
	const Vector3 C{ _capsule02.startPos };
	const Vector3 D{ _capsule02.endPos };

	// 点Pに最も近い、線分start-end上の点を返す
	const auto closestPointOnSegment = [](const Vector3& _P, const Vector3& _start, const Vector3& _end)
		{
			const Vector3 direction{ _end - _start };
			const float lengthSquared{ direction.LengthSquared() };

			if (lengthSquared <= Math::EPSILON * Math::EPSILON) return _start; // 線分の長さがほぼ0なら点として扱う
			const float t{ std::clamp(Vector3::Dot(_P - _start, direction) / lengthSquared, 0.0f, 1.0f) }; // 最近点までの割合
			return _start + direction * t;
		};

	// 最初の候補としてAとCD上でAに最も近い点
	Vector3 Q01{ A };
	Vector3 Q02{ closestPointOnSegment(A, C, D) };
	float minDistanceSquared{ Vector3::DistanceSquared(Q01, Q02) };

	// よりよい組が見つかったら距離と両方の点を更新
	const auto consider = [&](const Vector3& _point01, const Vector3& _point02)
		{
			const float distanceSquared{ Vector3::DistanceSquared(_point01, _point02) };
			if (distanceSquared < minDistanceSquared)
			{
				minDistanceSquared = distanceSquared;
				Q01 = _point01;
				Q02 = _point02;
			}
		};

	// 残り3つの端点候補
	consider(B, closestPointOnSegment(B, C, D));
	consider(closestPointOnSegment(C, A, B), C);
	consider(closestPointOnSegment(D, A, B), D);

	// 両方の線分の途中が最近点になる候補
	const Vector3 u{ B - A };
	const Vector3 v{ D - C };
	const Vector3 w{ A - C };

	// ほぼ平行な線分でも計算が崩れにくいよう、割合はdoubleで計算
	const auto dotD = [](const Vector3& lhs, const Vector3& rhs)
		{
			return static_cast<double>(lhs.x) * rhs.x + static_cast<double>(lhs.y) * rhs.y + static_cast<double>(lhs.z) * rhs.z;
		};

	const double a{ dotD(u, u) };
	const double b{ dotD(u, v) };
	const double c{ dotD(v, v) };
	const double d{ dotD(u, w) };
	const double e{ dotD(v, w) };
	const double denominator{ a * c - b * b };

	// 平行または長さ0ならこの候補は使わない　その場合の端点はもう調べてある
	if (denominator > 0.0f)
	{
		const double s{ (b * e - c * d) / denominator };
		const double t{ (a * e - b * d) / denominator };

		// 無限直線上の最近点が、両方の「線分内」にあるときだけ採用
		if (s >= 0.0 && s <= 1.0 && t >= 0.0 && t <= 1.0) consider(A + u * static_cast<float>(s), C + v * static_cast<float>(t));
	}

	// カプセルとして半径比較
	const float radiusSum{ _capsule01.radius + _capsule02.radius };
	return minDistanceSquared <= radiusSum * radiusSum;
}

bool CollisionSystem::Intersect(Capsule _capsule, Box _box)
{
	if (!_capsule.IsValid() || !_box.IsValid()) return false;
	// Boxの回転を打ち消しカプセルの軸線分をBoxのローカル空間へ移す
	const Quaternion inverseRotation{ Vector4{-_box.rotation.x, -_box.rotation.y, -_box.rotation.z, _box.rotation.w} };
	const Vector3 a{ inverseRotation.RotateVector(_capsule.startPos - _box.center) };
	const Vector3 b{ inverseRotation.RotateVector(_capsule.endPos - _box.center) };
	const Vector3 direction{ b - a };

	const std::array<float, 3> start{ a.x, a.y, a.z };
	const std::array<float, 3> dir{ direction.x, direction.y, direction.z };
	const std::array<float, 3> half{ _box.halfSize.x, _box.halfSize.y, _box.halfSize.z };

	// 軸線分上の点 P(t) とBoxとの距離の二乗  t=0始点 t=1終点
	const auto distanceSquaredAt = [&](float t)
		{
			const Vector3 point{ a + direction * t };
			const Vector3 nearest{
				std::clamp(point.x, -_box.halfSize.x, _box.halfSize.x),
				std::clamp(point.y, -_box.halfSize.y, _box.halfSize.y),
				std::clamp(point.z, -_box.halfSize.z, _box.halfSize.z)
			};
			return (point - nearest).LengthSquared();
		};

	// 線分がBoxの各面の延長平面を横切るtを集めるその境界で距離関数の式が切り替わる。
	std::array<float, 8> cuts{};
	int cutCount{ 2 };
	cuts[0] = 0.0f;
	cuts[1] = 1.0f;

	for (int axis = 0; axis < 3; axis++)
	{
		if (dir[axis] == 0.0f) continue;

		for (int sign : { -1, 1 })
		{
			const float plane{ static_cast<float>(sign) * half[axis] };
			const float t{ (plane - start[axis]) / dir[axis] };

			if (t > 0.0f && t < 1.0f)
			{
				cuts[cutCount++] = t;
			}
		}
	}

	std::sort(cuts.begin(), cuts.begin() + cutCount);

	// 境界点を候補にする。
	float minDistanceSquared{ distanceSquaredAt(cuts[0]) };
	for (int i = 1; i < cutCount; i++)
	{
		minDistanceSquared = std::min(minDistanceSquared, distanceSquaredAt(cuts[i]));
	}

	// 各区間内では「Boxの外側にある軸」が変わらず距離の二乗は二次関数になる その最小位置も調べる
	for (int i = 0; i + 1 < cutCount; i++)
	{
		const float left{ cuts[i] };
		const float right{ cuts[i + 1] };
		if (right <= left) continue;

		const float middle{ (left + right) * 0.5f };
		float numerator{ 0.0f };
		float denominator{ 0.0f };

		for (int axis = 0; axis < 3; axis++)
		{
			const float valueAtMiddle{ start[axis] + dir[axis] * middle };

			float nearestFace{};
			if (valueAtMiddle < -half[axis]) nearestFace = -half[axis];
			else if (valueAtMiddle > half[axis]) nearestFace = half[axis];
			else continue; // この軸ではBoxの内側

			numerator += dir[axis] * (start[axis] - nearestFace);
			denominator += dir[axis] * dir[axis];
		}

		if (denominator > 0.0f)
		{
			const float nearestT{ std::clamp(-numerator / denominator, left, right) };
			minDistanceSquared = std::min(minDistanceSquared, distanceSquaredAt(nearestT));
		}
	}

	// 表面がちょうど触れる場合もtrue。
	return minDistanceSquared <= _capsule.radius * _capsule.radius;
}

bool CollisionSystem::Intersect(Capsule _capsule, Sphere _sphere)
{
	if (!_capsule.IsValid() || !_sphere.IsValid()) return false;

	const Vector3 A{ _capsule.startPos };
	const Vector3 B{ _capsule.endPos };
	const Vector3 P{ _sphere.center }; 
	Vector3 Q{Vector3::Zero}; // 最近点
	Vector3 dir{ Vector3::FromTo(A, B) }; // capsule間のベクトル

	// 0除算を避けるために長さがない場合は始点とする
	if (dir.LengthSquared() < Math::EPSILON)
	{
		Q = A;
	}
	else
	{
		// カプセル始点->球までのベクトルを出し。カプセル間のベクトルと内積を取って投影してどの位置にあるのかをみる。そしてそれを割合にする(Dot(P-A, dir)ですでに1回dirがかかっているので二乗で割る)
		const float t{ std::clamp(Vector3::Dot(P - A, dir) / dir.LengthSquared(), 0.0f, 1.0f) };
		Q = A + t * dir;
	}

	// 最近点から級までの長さがカプセルの半径と級の半径を合計した距離より短いか
	return std::abs(Vector3::FromTo(Q, P).LengthSquared()) <= (_capsule.radius + _sphere.radius) * (_capsule.radius + _sphere.radius);
}

