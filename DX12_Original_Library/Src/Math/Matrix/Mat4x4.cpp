#include <cmath>
#include "Mat4x4.h"

// 全て0で初期化する
Mat4x4::Mat4x4()
{
	rows[0] = Vector4::Zero;
	rows[1] = Vector4::Zero;
	rows[2] = Vector4::Zero;
	rows[3] = Vector4::Zero;
}

//　行で初期化する
Mat4x4::Mat4x4(const Vector4& _row0, const Vector4& _row1, const Vector4& _row2, const Vector4& _row3)
{
	rows[0] = _row0;
	rows[1] = _row1;
	rows[2] = _row2;
	rows[3] = _row3;
}

// 単位行列
const Mat4x4 Mat4x4::Identity
{
	Mat4x4
	{
		Vector4{1.0f, 0.0f, 0.0f, 0.0f},
		Vector4{0.0f, 1.0f, 0.0f, 0.0f},
		Vector4{0.0f, 0.0f, 1.0f, 0.0f},
		Vector4{0.0f, 0.0f, 0.0f, 1.0f},
	}
};

// 平行移動行列を作成する
Mat4x4 Mat4x4::MakeTranslation(const Vector3& _pos)
{
	Mat4x4 result{};
	result.rows[0].x = 1.0f, result.rows[0].y = 0.0f, result.rows[0].z = 0.0f, result.rows[0].w = 0.0f;
	result.rows[1].x = 0.0f, result.rows[1].y = 1.0f, result.rows[1].z = 0.0f, result.rows[1].w = 0.0f;
	result.rows[2].x = 0.0f, result.rows[2].y = 0.0f, result.rows[2].z = 1.0f, result.rows[2].w = 0.0f;
	result.rows[3].x = _pos.x, result.rows[3].y = _pos.y, result.rows[3].z = _pos.z, result.rows[3].w = 1.0f;

	return result;
}

// X軸回転行列
Mat4x4 Mat4x4::MakeRotationX(float _radians)
{
	float cosCache{ std::cosf(_radians) }; // cos用キャッシュ
	float sinCache{ std::sinf(_radians) }; // sin用キャッシュ

	Mat4x4 result
	{
		Vector4{1.0f, 0.0f, 0.0f, 0.0f},
		Vector4{0.0f, cosCache, sinCache, 0.0f},
		Vector4{0.0f, -sinCache, cosCache, 0.0f},
		Vector4{0.0f, 0.0f, 0.0f, 1.0f}
	};

	return result;
}

// Y軸回転行列
Mat4x4 Mat4x4::MakeRotationY(float _radians)
{
	float cosCache{ std::cosf(_radians) }; // cos用キャッシュ
	float sinCache{ std::sinf(_radians) }; // sin用キャッシュ

	Mat4x4 result
	{
		Vector4{cosCache, 0.0f, sinCache, 0.0f},
		Vector4{0.0f, 1.0f, 0.0f, 0.0f},
		Vector4{-sinCache, 0.0f, cosCache, 0.0f},
		Vector4{ 0.0f, 0.0f, 0.0f, 1.0f}
	};

	return result;
}

// Z軸回転行列
Mat4x4 Mat4x4::MakeRotationZ(float _radians)
{
	float cosCache{ std::cosf(_radians) }; // cos用キャッシュ
	float sinCache{ std::sinf(_radians) }; // sin用キャッシュ

	Mat4x4 result
	{
		Vector4{cosCache, sinCache, 0.0f, 0.0f},
		Vector4{-sinCache, cosCache, 0.0f, 0.0f},
		Vector4{0.0f, 0.0f, 1.0f, 0.0f},
		Vector4{0.0f, 0.0f, 0.0f, 1.0f}
	};

	return result;
}

// 拡縮行列
Mat4x4 Mat4x4::MakeScaling(const Vector3& _scale)
{
	Mat4x4 result{};
	result.rows[0].x = _scale.x, result.rows[0].y = 0.0f, result.rows[0].z = 0.0f, result.rows[0].w = 0.0f;
	result.rows[1].x = 0.0f, result.rows[1].y = _scale.y, result.rows[1].z = 0.0f, result.rows[1].w = 0.0f;
	result.rows[2].x = 0.0f, result.rows[2].y = 0.0f, result.rows[2].z = _scale.z, result.rows[2].w = 0.0f;
	result.rows[3].x = 0.0f, result.rows[3].y = 0.0f, result.rows[3].z = 0.0f, result.rows[3].w = 1.0f;

	return result;
}

// View行列
Mat4x4 Mat4x4::MakeLookAt(const Vector3& _eyePos, const Vector3& _targetPos, const Vector3& _up)
{
	// カメラの軸を求める
	Vector3 zAxis{ Vector3::Normalized(_targetPos - _eyePos) }; // カメラが見ている方向
	Vector3 xAxis{ Vector3::Normalized(Vector3::Cross(_up, zAxis)) }; // カメラの右方向
	Vector3 yAxis{ Vector3::Cross(zAxis, xAxis) }; // カメラの実際の上方向

	// 結果
	Mat4x4 result
	{
		Vector4{xAxis.x, yAxis.x, zAxis.x, 0.0f},
		Vector4{xAxis.y, yAxis.y, zAxis.y, 0.0f},
		Vector4{xAxis.z, yAxis.z, zAxis.z, 0.0f},
		Vector4{-Vector3::Dot(xAxis, _eyePos), -Vector3::Dot(yAxis, _eyePos), -Vector3::Dot(zAxis, _eyePos), 1.0f},
	};

	return result;
}

// 透視行列
Mat4x4 Mat4x4::MakePerspective(float _fovY, float _aspect, float _nearZ, float _farZ)
{
	float yScale{ 1.0f / std::tanf(_fovY / 2.0f) };
	float xScale{ yScale / _aspect };

	Mat4x4 result
	{
		Vector4{xScale, 0.0f, 0.0f, 0.0f},
		Vector4{0.0f, yScale, 0.0f, 0.0f},
		Vector4{0.0f, 0.0f, _farZ / (_farZ - _nearZ), 1.0f},
		Vector4{0.0f, 0.0f, -_nearZ*_farZ / (_farZ - _nearZ), 0.0f},
	};

	return result;
}

// 転置行列の作成
Mat4x4 Mat4x4::MakeTransposed(const Mat4x4& _other)
{
	Mat4x4 result
	{
		Vector4{_other.rows[0].x, _other.rows[1].x, _other.rows[2].x, _other.rows[3].x},
		Vector4{_other.rows[0].y, _other.rows[1].y, _other.rows[2].y, _other.rows[3].y},
		Vector4{_other.rows[0].z, _other.rows[1].z, _other.rows[2].z, _other.rows[3].z},
		Vector4{_other.rows[0].w, _other.rows[1].w, _other.rows[2].w, _other.rows[3].w}
	};

	return result;
}

Vector4 Mat4x4::Mul(const Vector4& _vec, const Mat4x4& _mat)
{
	Vector4 result{}; // 結果を返すためのベクトル

	result = _vec.x * _mat.rows[0]
		+ _vec.y * _mat.rows[1]
		+ _vec.z * _mat.rows[2]
		+ _vec.w * _mat.rows[3];

	return result;
}

// 位置に変換する
Vector3 Mat4x4::TransformPoint(const Vector3& _point) const
{
	Vector4 convert{Vector4::FromPosition(_point)}; // Vec4へ変換(w = 1)
	Vector4 mulTransformMat{ Mul(convert, (*this))}; // 行列変換
	return mulTransformMat.ToVec3(); // Vec3に戻す
}

// 方向変換
Vector3 Mat4x4::TransformDirection(const Vector3& _dir) const
{
	Vector4 convert{ Vector4::FromDirection(_dir) }; // Vec4へ変換(w = 0)
	Vector4 mulTransformMat{ Mul(convert, (*this))}; // 行列変換
	return mulTransformMat.ToVec3(); // Vec3に戻す
}

// 行列同士の乗算
Mat4x4 Mat4x4::operator *(const Mat4x4& _other) const
{
	Mat4x4 result{};

	for (int i = 0; i < Math::MATRIX_SIZE; i++)
	{
		result.rows[i] =
			  rows[i].x * _other.rows[0]
			+ rows[i].y * _other.rows[1]
			+ rows[i].z * _other.rows[2]
			+ rows[i].w * _other.rows[3];
	}

	return result;
}

