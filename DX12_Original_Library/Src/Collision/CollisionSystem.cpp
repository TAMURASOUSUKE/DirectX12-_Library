#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include "../Math/TSMath.h"
#include "CollisionSystem.h"

namespace
{
	// 指定した点に最も近い、線分上の点を求める。
	// CapsuleとSphereなど「線分を中心軸に持つ形状」の距離計算で使用する。
	Vector3 ClosestPointOnSegment(const Vector3& point, const Vector3& start, const Vector3& end)
	{
		// 始点から終点へ向かう線分の方向ベクトル。
		const Vector3 direction{ end - start };
		// 線分の長さの二乗。退化判定と投影割合tの計算に使う。
		const float lengthSquared{ direction.LengthSquared() };
		// 始点と終点がほぼ同じなら線分は点なので、唯一の点である始点を返す
		if (lengthSquared <= Math::EPSILON * Math::EPSILON) return start;
		// 点を線分方向へ投影し、最近点が始点から終点までの何割の位置か求める clampにより、直線への投影位置が線分外なら始点または終点に制限する
		const float t{ std::clamp(Vector3::Dot(point - start, direction) / lengthSquared, 0.0f, 1.0f) };
		// 始点から線分方向へtの割合だけ進め、線分上の最近点を復元する。
		return start + direction * t;
	}

	// 指定した方向に垂直な単位ベクトルを一つ求める 最近点同士が一致して通常の押し戻し方向を決められない場合の退避方向に使う。
	Vector3 PerpendicularTo(const Vector3& direction)
	{
		// まずUpとの外積からdirectionに垂直な方向を作る。
		Vector3 normal{ Vector3::Cross(direction, Vector3::Up) };
		// directionとUpが平行なら外積がゼロになるため、Rightを基準に作り直す。
		if (normal.LengthSquared() <= Math::EPSILON * Math::EPSILON) normal = Vector3::Cross(direction, Vector3::Right);
		// direction自体もゼロなら垂直方向を決められないため、固定方向を返す。
		if (normal.LengthSquared() <= Math::EPSILON * Math::EPSILON) return Vector3::Right;
		// 押し戻し距離へ方向ベクトルの長さが影響しないよう単位化する。
		normal.Normalize();
		return normal;
	}
	
	// 2本の線分について、それぞれの線分上にある最近点を保持する。
	struct SegmentPair
	{
		// 1本目の線分上にある最近点。
		Vector3 first{};
		// 2本目の線分上にある最近点。
		Vector3 second{};
	};

	// 2本の線分の最近点を一つずつ求める。点に潰れた線分と平行線分も扱う。
	// P(s)=a+(b-a)s、Q(t)=c+(d-c)tと表し、|P(s)-Q(t)|が最小になるsとtを求める。
	SegmentPair ClosestSegmentPair(const Vector3& a, const Vector3& b, const Vector3& c, const Vector3& d)
	{
		// 1本目と2本目の方向ベクトル。
		const Vector3 u{ b - a };
		const Vector3 v{ d - c };
		// 2本目の始点cから1本目の始点aへ向かうベクトル。
		const Vector3 w{ a - c };

		// ほぼ平行な線分でも計算が崩れにくいよう、割合はdoubleで計算
		const auto dot = [](const Vector3& lhs, const Vector3& rhs)
			{
				return static_cast<double>(lhs.x) * rhs.x + static_cast<double>(lhs.y) * rhs.y + static_cast<double>(lhs.z) * rhs.z;
			};

		// 1本目の線分の長さの二乗
		const double uu{ dot(u, u) };

		// 2本の線分方向の内積
		const double uv{ dot(u, v) };

		// 2本目の線分の長さの二乗
		const double vv{ dot(v, v) };

		// 始点間ベクトルを1本目の方向へ投影するための内積
		const double uw{ dot(u, w) };

		// 始点間ベクトルを2本目の方向へ投影するための内積
		const double vw{ dot(v, w) };
		// 長さの二乗をゼロとみなす境界値。
		const double tiny{ static_cast<double>(Math::EPSILON) * Math::EPSILON };
		// 各線分上にある最近点の割合。0が始点、1が終点。
		double s{ 0.0 };
		double t{ 0.0 };

		// 両方の線分が点に潰れている場合、各始点がそのまま最近点になる。
		if (uu <= tiny && vv <= tiny) return { a, c };
		// 1本目だけが点なら、点aを2本目へ投影してtだけを求める。
		if (uu <= tiny) t = std::clamp(vw / vv, 0.0, 1.0);
		// 2本目だけが点なら、点cを1本目へ投影してsだけを求める。
		// wはa-cなので、aからcへの方向へ直すためuwに負号を付ける。
		else if (vv <= tiny) s = std::clamp(-uw / uu, 0.0, 1.0);
		else
		{
			// 2本の方向がどれだけ平行に近いかを表す値。
			// 非平行なら連立方程式から、1本目側の最近点割合sを求められる。
			const double denominator{ uu * vv - uv * uv };
			if (denominator > tiny) s = std::clamp((uv * vw - vv * uw) / denominator, 0.0, 1.0);

			// 求めたsに対して、2本目の直線上で最も近くなるtを求める。
			// 平行時はs=0のままなので、1本目の始点aに最も近いtから調べる。
			t = (uv * s + vw) / vv;

			// tが始点より外ならt=0に固定し、点cに最も近いsを求め直す。
			if (t < 0.0)
			{
				t = 0.0;
				s = std::clamp(-uw / uu, 0.0, 1.0);
			}
			// tが終点より外ならt=1に固定し、点dに最も近いsを求め直す。
			else if (t > 1.0)
			{
				t = 1.0;
				s = std::clamp((uv - uw) / uu, 0.0, 1.0);
			}
		}
		// 割合sとtから、各線分上の最近点を復元して返す。
		return { a + u * static_cast<float>(s), c + v * static_cast<float>(t) };
	}
}

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
		// カプセル始点->球までのベクトルを出し、カプセル間のベクトルと内積を取って投影してどの位置にあるのかをみる。そしてそれを割合にする(Dot(P-A, dir)ですでに1回dirがかかっているので二乗で割る)
		const float t{ std::clamp(Vector3::Dot(P - A, dir) / dir.LengthSquared(), 0.0f, 1.0f) };
		Q = A + t * dir;
	}

	// 最近点から級までの長さがカプセルの半径と級の半径を合計した距離より短いか
	return std::abs(Vector3::FromTo(Q, P).LengthSquared()) <= (_capsule.radius + _sphere.radius) * (_capsule.radius + _sphere.radius);
}

bool CollisionSystem::ComputePushOut(Rect _movable, Rect _obstacle, Vector2& _outMove)
{
	_outMove = Vector2::Zero;
	if (!Intersect(_movable, _obstacle)) return false;

	const Vector2 aMin{ _movable.GetMinPos() }; // 押し戻される方の最小
	const Vector2 aMax{ _movable.GetMaxPos() }; // 押し戻されるほうの最大
	const Vector2 bMin{ _obstacle.GetMinPos() }; // 固定されている方の最小
	const Vector2 bMax{ _obstacle.GetMaxPos() }; // 固定されている方の最大

	// 外へ出す候補
	const std::array<Vector2, 4> candidates{
		Vector2{ bMin.x - aMax.x, 0.0f },
		Vector2{ bMax.x - aMin.x, 0.0f },
		Vector2{ 0.0f, bMin.y - aMax.y },
		Vector2{ 0.0f, bMax.y - aMin.y }
	};
	_outMove = candidates[0];

	// 状態を見て戻す量を渡す
	for (const Vector2& candidate : candidates)
	{
		if (candidate.LengthSquared() < _outMove.LengthSquared()) _outMove = candidate;
	}
	return true;
}

bool CollisionSystem::ComputePushOut(Sphere _movable, Sphere _obstacle, Vector3& _outMove)
{
	// 非衝突時でも呼び出し側へ古い移動量を残さない。
	_outMove = Vector3::Zero;
	if (!Intersect(_movable, _obstacle)) return false;

	const Vector3 difference{ _movable.center - _obstacle.center }; // 障害物中心から移動側中心への方向
	const float distanceSquared{ difference.LengthSquared() }; // 中心一致判定用の距離の二乗
	// 2球がちょうど接触するときに必要な中心間距離。
	const float radiusSum{ _movable.radius + _obstacle.radius };
	// 中心が一致すると方向を決められず距離による除算もできないため、固定方向へ退避する。
	if (distanceSquared <= Math::EPSILON * Math::EPSILON)
	{
		_outMove = Vector3::Right * radiusSum; // 中心一致時は方向を固定する : 仮で右
		return true;
	}
	// differenceを単位方向に直し、半径合計-distanceのめり込み量だけ押し戻す。
	const float distance{ std::sqrt(distanceSquared) };
	// 中心間の距離を距離で割ることで法線とする radiusSum - distanceは押し戻す量
	_outMove = difference * (std::max(0.0f, radiusSum - distance) / distance);
	return true;
}

bool CollisionSystem::ComputePushOut(Sphere _movable, Box _obstacle, Vector3& _outMove)
{
	_outMove = Vector3::Zero;
	if (!Intersect(_movable, _obstacle)) return false;

	// Boxの逆回転で球中心をBoxローカル空間へ移し、回転のないAABBとして計算する。
	const Quaternion inverse{ Vector4{-_obstacle.rotation.x, -_obstacle.rotation.y, -_obstacle.rotation.z, _obstacle.rotation.w } };
	const Vector3 center{ inverse.RotateVector(_movable.center - _obstacle.center) };

	// 球中心の各座標をBox範囲へ収め、Box上で球に最も近い点を求める。
	const Vector3 nearest{
		std::clamp(center.x, -_obstacle.halfSize.x, _obstacle.halfSize.x),
		std::clamp(center.y, -_obstacle.halfSize.y, _obstacle.halfSize.y),
		std::clamp(center.z, -_obstacle.halfSize.z, _obstacle.halfSize.z)
	};


	const Vector3 difference{ center - nearest }; // Box上の最近点から球中心へのローカル方向
	const float distanceSquared{ difference.LengthSquared() };
	Vector3 localMove{};
	// 球中心がBox外なら、最近点から球中心へ向かう方向にめり込み量だけ戻す。
	if (distanceSquared > Math::EPSILON * Math::EPSILON)
	{
		const float distance{ std::sqrt(distanceSquared) };
		localMove = difference * (std::max(0.0f, _movable.radius - distance) / distance); // 円の半径を考慮して押し戻し量を出す
	}
	else
	{
		// 球中心が箱の内側・表面上ならdifferenceがゼロになる。
		// 6面への移動量を比較し、最も近い面から球全体を出す。
		const std::array<float, 3> p{ center.x, center.y, center.z };
		const std::array<float, 3> h{ _obstacle.halfSize.x, _obstacle.halfSize.y, _obstacle.halfSize.z };
		float best{ std::numeric_limits<float>::infinity() };
		int bestAxis{ 0 };
		for (int axis = 0; axis < 3; axis++)
		{
			// 正負それぞれの面を越え、球の反対側までBox外へ出す移動量。
			const float positive{ h[axis] + _movable.radius - p[axis] };
			const float negative{ -h[axis] - _movable.radius - p[axis] };
			if (std::abs(positive) < std::abs(best)) 
			{
				best = positive; 
				bestAxis = axis;
			}
			if (std::abs(negative) < std::abs(best))
			{
				best = negative; 
				bestAxis = axis;
			}
		}
		// 最小移動となる一軸だけへ移動量を設定する
		if (bestAxis == 0) localMove.x = best;
		if (bestAxis == 1) localMove.y = best;
		if (bestAxis == 2) localMove.z = best;
	}
	_outMove = _obstacle.rotation.RotateVector(localMove); // ローカルの移動量をワールド空間へ戻す
	return true;
}

bool CollisionSystem::ComputePushOut(Capsule _movable, Sphere _obstacle, Vector3& _outMove)
{
	_outMove = Vector3::Zero;
	if (!Intersect(_movable, _obstacle)) return false;

	const Vector3 closest{ ClosestPointOnSegment(_obstacle.center, _movable.startPos, _movable.endPos) }; // 球中心に最も近いカプセル軸上の点
	const Vector3 difference{ closest - _obstacle.center }; // 球中心からカプセル軸上の最近点への方向
	const float distanceSquared{ difference.LengthSquared() };
	const float radiusSum{ _movable.radius + _obstacle.radius };

	// 最近点と球中心が一致すると通常の方向を作れないため、カプセル軸に垂直な方向へ退避する。
	if (distanceSquared <= Math::EPSILON * Math::EPSILON)
	{
		_outMove = PerpendicularTo(_movable.endPos - _movable.startPos) * radiusSum;
		return true;
	}

	const float distance{ std::sqrt(distanceSquared) };
	// 最近点同士を結ぶ単位方向へ、半径合計から実距離を引いためり込み量だけ戻す。
	_outMove = difference * (std::max(0.0f, radiusSum - distance) / distance);
	return true;
}

bool CollisionSystem::ComputePushOut(Capsule _movable, Capsule _obstacle, Vector3& _outMove)
{
	_outMove = Vector3::Zero;
	if (!Intersect(_movable, _obstacle)) return false;

	// 2本のカプセル中心軸について、それぞれの線分上の最近点を求める。
	const SegmentPair pair{ ClosestSegmentPair(_movable.startPos, _movable.endPos, _obstacle.startPos, _obstacle.endPos) };
	// 障害物側の最近点から移動側の最近点へ向かう方向。
	const Vector3 difference{ pair.first - pair.second };
	const float distanceSquared{ difference.LengthSquared() };
	const float radiusSum{ _movable.radius + _obstacle.radius };
	// 最近点同士が一致するとdifferenceがゼロになり、通常の分離方向を作れない。
	if (distanceSquared <= Math::EPSILON * Math::EPSILON)
	{
		const Vector3 firstAxis{ _movable.endPos - _movable.startPos };
		const Vector3 secondAxis{ _obstacle.endPos - _obstacle.startPos };
		// 非平行なら、両方の軸に垂直な外積方向を退避方向にする。
		Vector3 normal{ Vector3::Cross(firstAxis, secondAxis) };

		// 平行なら外積もゼロになるため、長い方の軸に垂直な方向を使う。
		if (normal.LengthSquared() <= Math::EPSILON * Math::EPSILON) normal = PerpendicularTo(firstAxis.LengthSquared() > secondAxis.LengthSquared() ? firstAxis : secondAxis);
		else normal.Normalize();

		_outMove = normal * radiusSum;
		return true;
	}
	const float distance{ std::sqrt(distanceSquared) };
	// 最近点同士を結ぶ単位方向へ、半径合計から実距離を引いためり込み量だけ戻す。
	_outMove = difference * (std::max(0.0f, radiusSum - distance) / distance);
	return true;
}

bool CollisionSystem::ComputePushOut(Box _movable, Box _obstacle, Vector3& _outMove)
{
	_outMove = Vector3::Zero;
	if (!Intersect(_movable, _obstacle)) return false;

	// 各BoxのローカルX・Y・Z軸を回転させ、ワールド空間での向きを求める。
	const std::array<Vector3, 3> axesA{
		_movable.rotation.RotateVector(Vector3::Right),
		_movable.rotation.RotateVector(Vector3::Up),
		_movable.rotation.RotateVector(Vector3::Forward)
	};
	const std::array<Vector3, 3> axesB{
		_obstacle.rotation.RotateVector(Vector3::Right),
		_obstacle.rotation.RotateVector(Vector3::Up),
		_obstacle.rotation.RotateVector(Vector3::Forward)
	};

	// 各ローカル軸方向の半分の長さ。候補軸への投影半径を求めるときに使う。
	const std::array<float, 3> halfA{ _movable.halfSize.x, _movable.halfSize.y, _movable.halfSize.z };
	const std::array<float, 3> halfB{ _obstacle.halfSize.x, _obstacle.halfSize.y, _obstacle.halfSize.z };
	// 移動側Boxの中心から障害物Boxの中心へ向かうベクトル。
	const Vector3 centerDelta{ _obstacle.center - _movable.center };
	// 全候補軸のうち、最も浅い重なりとその分離方向を保持する。
	float smallestOverlap{ std::numeric_limits<float>::infinity() };
	Vector3 moveDirection{ Vector3::Right };

	// 一つの分離軸候補へ両Boxを投影し、その軸上の重なり量を調べる。
	const auto consider = [&](Vector3 axis)
		{
			const float lengthSquared{ axis.LengthSquared() };
			if (lengthSquared <= Math::EPSILON * Math::EPSILON) return; // 平行軸の外積は無効
			axis /= std::sqrt(lengthSquared); // 異なる軸の深さを比較できるよう単位化
			const float signedDistance{ Vector3::Dot(centerDelta, axis) }; // 候補軸上の符号付き中心間距離
			float radiusA{ 0.0f };
			float radiusB{ 0.0f };
			// Boxの3本の半辺を候補軸へ投影し、絶対値の合計から投影半径を求める。
			for (int i = 0; i < 3; i++)
			{
				radiusA += halfA[i] * std::abs(Vector3::Dot(axesA[i], axis));
				radiusB += halfB[i] * std::abs(Vector3::Dot(axesB[i], axis));
			}
			// 投影半径の合計から中心間距離を引いた値が、この軸上の重なり量。
			const float overlap{ std::max(0.0f, radiusA + radiusB - std::abs(signedDistance)) };
			// 最も浅い重なりの軸が、最小移動で分離できるMTVになる。
			if (overlap < smallestOverlap)
			{
				smallestOverlap = overlap;
				moveDirection = signedDistance > 0.0f ? -axis : axis; // 障害物とは反対側を選ぶ
			}
		};

	// OBB同士のSATで必要な15軸を調べる。
	// 両Box自身の面法線6軸と、各軸同士の外積9軸。
	for (const Vector3& axis : axesA)
	{
		consider(axis);
	}

	for (const Vector3& axis : axesB)
	{
		consider(axis);
	}

	for (const Vector3& a : axesA)
	{
		for (const Vector3& b : axesB) 
		{
			consider(Vector3::Cross(a, b));
		}
	}

	_outMove = moveDirection * smallestOverlap; // 最も浅い重なり方向へ必要量だけ移動する
	return true;
}

bool CollisionSystem::ComputePushOut(Capsule _movable, Box _obstacle, Vector3& _outMove)
{
	_outMove = Vector3::Zero;
	if (!_movable.IsValid() || !_obstacle.IsValid()) return false;

	// Boxの逆回転でカプセル中心軸をBoxローカル空間へ移す。
	// 回転したOBBを、軸に沿ったAABBとして計算できるようにする。
	const Quaternion inverse{ Vector4{-_obstacle.rotation.x, -_obstacle.rotation.y, -_obstacle.rotation.z, _obstacle.rotation.w } };
	const Vector3 a{ inverse.RotateVector(_movable.startPos - _obstacle.center) };
	const Vector3 b{ inverse.RotateVector(_movable.endPos - _obstacle.center) };
	const Vector3 direction{ b - a }; // Boxローカル空間でのカプセル中心軸方向
	// X・Y・Zを同じループで処理できるよう配列化する。
	const std::array<float, 3> start{ a.x, a.y, a.z };
	const std::array<float, 3> dir{ direction.x, direction.y, direction.z };
	const std::array<float, 3> half{ _obstacle.halfSize.x, _obstacle.halfSize.y, _obstacle.halfSize.z };

	// 線分上の割合tにある点とAABBとの距離の二乗を求める。t=0が始点、t=1が終点。
	// 座標をBox範囲へclampすると、その点に最も近いBox上の点になる。
	const auto distanceSquaredAt = [&](float t)
		{
			const Vector3 point{ a + direction * t };
			const Vector3 nearest{
				std::clamp(point.x, -half[0], half[0]),
				std::clamp(point.y, -half[1], half[1]),
				std::clamp(point.z, -half[2], half[2])
			};
			return (point - nearest).LengthSquared();
		};

	// 線分上で「最も近いBoxの面」が変わる境界tを保存する。
	// Boxの6面と線分の両端を合わせ、境界は最大8個になる。
	std::array<float, 8> cuts{};
	int cutCount{ 2 };
	// 線分の始点t=0と終点t=1は必ず調査対象に含める。
	cuts[0] = 0.0f;
	cuts[1] = 1.0f;
	for (int axis = 0; axis < 3; ++axis)
	{
		// この軸方向へ動かない線分は面を横切らず、除算もできないため飛ばす。
		if (std::abs(dir[axis]) <= Math::EPSILON) continue;
		// 各軸について、Boxのマイナス側とプラス側の面を調べる。
		for (const int sign : { -1, 1 })
		{
			// start+dir*t=sign*halfをtについて解き、線分が面を横切る割合を求める。
			const float t{ (static_cast<float>(sign) * half[axis] - start[axis]) / dir[axis] };
			// 線分内の境界だけ追加する。両端の0と1は登録済み。
			if (t > 0.0f && t < 1.0f) cuts[cutCount++] = t;
		}
	}
	// 始点から終点へ向かう順に並べ、隣り合う値を一つの計算区間として扱う。
	std::sort(cuts.begin(), cuts.begin() + cutCount);

	// 始点を最初の候補とし、よりBoxに近いtが見つかるたびに更新する。
	float nearestT{ cuts[0] };
	float minDistanceSquared{ distanceSquaredAt(nearestT) };

	const auto consider = [&](float t)
		{
			const float distanceSquared{ distanceSquaredAt(t) };
			if (distanceSquared < minDistanceSquared)
			{
				minDistanceSquared = distanceSquared;
				nearestT = t;
			}
		};

	// 面を横切る境界そのものも最短位置になる可能性があるため調べる。
	for (int i = 1; i < cutCount; i++)
	{
		consider(cuts[i]);
	}

	// 各区間の内部に存在する最短位置を調べる。
	// 区間内では各軸がBoxの内側か外側かが変わらず、距離二乗は二次式になる。
	for (int i = 0; i + 1 < cutCount; i++)
	{
		const float left{ cuts[i] };
		const float right{ cuts[i + 1] };

		// 角を同時に横切ると境界が重複するため、幅のない区間は飛ばす。
		if (right <= left) continue;

		// 区間中央を代表点にし、各軸がBoxのどの面より外側かを判定する。
		const float middle{ (left + right) * 0.5f };
		float numerator{ 0.0f };
		float denominator{ 0.0f };

		for (int axis = 0; axis < 3; axis++)
		{
			const float value{ start[axis] + dir[axis] * middle };
			float face{};

			// Box外にある軸だけ距離へ寄与する。内側の軸は最近点と同じなので無視する。
			if (value < -half[axis]) face = -half[axis];
			else if (value > half[axis]) face = half[axis];
			else continue;

			// 距離二乗D(t)^2を微分した式の係数を集める。
			// 最小位置はt=-numerator/denominatorで求められる。
			numerator += dir[axis] * (start[axis] - face);
			denominator += dir[axis] * dir[axis];
		}

		// 求めた最小位置を現在の区間内へ制限して比較する。
		if (denominator > 0.0f) consider(std::clamp(-numerator / denominator, left, right));
	}

	// 中心軸線分とBoxの最短距離がカプセル半径より大きければ非衝突。
	const float radiusSquared{ _movable.radius * _movable.radius };
	if (minDistanceSquared > radiusSquared) return false;

	Vector3 localMove{};
	// 軸の最近点がBox外なら、Box上の最近点から離れる方向へ押し戻す。
	if (minDistanceSquared > Math::EPSILON * Math::EPSILON)
	{
		const Vector3 point{ a + direction * nearestT };
		const Vector3 nearest{
			std::clamp(point.x, -half[0], half[0]),
			std::clamp(point.y, -half[1], half[1]),
			std::clamp(point.z, -half[2], half[2])
		};
		const float distance{ std::sqrt(minDistanceSquared) };
		// 単位分離方向へ、カプセル半径から実距離を引いためり込み量だけ移動する。
		localMove = (point - nearest) * (std::max(0.0f, _movable.radius - distance) / distance);
		localMove = (point - nearest) * (std::max(0.0f, _movable.radius - distance) / distance);
	}
	else
	{
		// 軸線分が箱と交差すると分離方向を作れないため、線分全体＋半径を最短面の外へ出す。
		float best{ std::numeric_limits<float>::infinity() };
		int bestAxis{ 0 };
		for (int axis = 0; axis < 3; ++axis)
		{
			// 線分の両端をこの軸へ投影し、軸方向の最小値と最大値を求める。
			const float end{ start[axis] + dir[axis] };
			const float segmentMin{ std::min(start[axis], end) };
			const float segmentMax{ std::max(start[axis], end) };
			// 線分全体と半径を正負それぞれの面外へ出す移動量。
			const float positive{ half[axis] + _movable.radius - segmentMin };
			const float negative{ -half[axis] - _movable.radius - segmentMax };

			if (std::abs(positive) < std::abs(best)) { best = positive; bestAxis = axis; }
			if (std::abs(negative) < std::abs(best)) { best = negative; bestAxis = axis; }
		}
		// 最小移動となる一軸だけへ移動量を設定する。
		if (bestAxis == 0) localMove.x = best;
		if (bestAxis == 1) localMove.y = best;
		if (bestAxis == 2) localMove.z = best;
	}
	_outMove = _obstacle.rotation.RotateVector(localMove); // ローカルの移動量をワールド空間へ戻す
	return true;
}

