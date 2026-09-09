#pragma once
#include "../MathConstant.h"
#include "../Vector/Vector3.h"
#include "../Vector/Vector4.h"

/// <summary>
/// 4x4の正方行列を定義する(DirectX12を使うので行優先に合わせる)
/// 左手系座標基準 : DirectXの仕様に合わせる
/// </summary>
struct Mat4x4
{
	// 全て0で初期化する
	constexpr Mat4x4()
	{
		rows[0] = Vector4::Zero;
		rows[1] = Vector4::Zero;
		rows[2] = Vector4::Zero;
		rows[3] = Vector4::Zero;
	}
	//　行で初期化する
	constexpr Mat4x4(const Vector4& _row0, const Vector4& _row1, const Vector4& _row2, const Vector4& _row3)
	{
		rows[0] = _row0;
		rows[1] = _row1;
		rows[2] = _row2;
		rows[3] = _row3;
	}
	~Mat4x4() = default;

	// 単位行列を返す
	static const Mat4x4 Identity;

	// 平行移動行列を作る。オブジェクトをワールド上の座標に配置する
	static Mat4x4 MakeTranslation(const Vector3& _pos);

	// X軸回転行列の作成
	static Mat4x4 MakeRotationX(float _radians);

	// Y軸回転行列の作成
	static Mat4x4 MakeRotationY(float _radians);

	// Z軸回転行列の作成
	static Mat4x4 MakeRotationZ(float _radians);

	// 拡縮行列の作成(各軸ごとに倍率を調整する)
	static Mat4x4 MakeScaling(const Vector3& _scale);

	/// <summary>
	/// カメラの位置、注視点、上方向からView行列を作る
	/// </summary>
	/// <param name="_eyePos">カメラの位置</param>
	/// <param name="_targetPos">注視点</param>
	/// <param name="_up">上方向</param>
	static Mat4x4 MakeLookAt(const Vector3& _eyePos, const Vector3& _targetPos, const Vector3& _up);

	/// <summary>
	/// 3D空間を2D画面に映すための投影投影行列を作成する
	/// </summary>
	/// <param name="_fovY">カメラの縦方向の視野角</param>
	/// <param name="_aspec">画面の横幅 / 縦幅(アスペクト比)</param>
	/// <param name="_nearZ">NearClip面までの距離</param>
	/// <param name="_farZ">FarClip面までの距離</param>
	/// <returns></returns>
	static Mat4x4 MakePerspective(float _fovY, float _aspect, float _nearZ, float _farZ);

	// 転置行列を作成
	static Mat4x4 MakeTransposed(const Mat4x4& _other);

	// 2D画面座標用正射影行列を作成する
	static Mat4x4 MakeOrthGraphic(float _width, float _height);
	// 3D空間・ShadowMap用正射影行列を作成する
	static Mat4x4 MakeOrthGraphic(float _width, float _height, float _nearZ, float _farZ);

	// Vector4と4x4行列を乗算する
	static Vector4 Mul(const Vector4& _vec, const Mat4x4& _mat);

	// 自身と引数を乗算し位置として変換する(w = 1つまり平行移動の影響を受ける)
	Vector3 TransformPoint(const Vector3& _point) const;

	// 自身と引数を乗算し方向として変換する(w = 0つまり平行移動の影響を受けない)
	Vector3 TransformDirection(const Vector3& _dir) const;

	// 演算

	Mat4x4 operator *(const Mat4x4& _other) const; // 行列同士の乗算

	Vector4 rows[Math::MATRIX_SIZE]; // Vec4を4行分持つ(行がSIMDレジスタと1対1対応しDotを再利用できるため)
};
