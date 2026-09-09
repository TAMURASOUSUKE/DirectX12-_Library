#include <Windows.h>
#include <iostream>
#include <variant>
#include <string>
#include "../Src/Graphics/ShadowSystem.h" // テストのため必要
#include "../Src/Facade/TSLib.h"

int testCount{ 0 };
int passCount{ 0 };


void Check(bool condition, const char* testName)
{
    testCount++;
    if (condition)
    {
        passCount++;
        std::cout << "[PASS] " << testName << std::endl;
    }
    else
    {
        std::cout << "[FAIL] " << testName << std::endl;
    }
}

// floatの近似比較
bool NearEqual(float a, float b)
{
    return std::abs(a - b) < Math::EPSILON;
}

// Vector3の近似
bool NearEqualVec3(const Vector3& a, const Vector3& b)
{
    return NearEqual(a.x, b.x)
        && NearEqual(a.y, b.y)
        && NearEqual(a.z, b.z);
}

// variantテスト用
struct Printer { std::string  operator()(int _n) { return "int!"; } std::string operator()(float _f) { return "float!"; } };

int main()
{
	// std::coutが出力するUTF-8文字列をWindowsコンソールにもUTF-8として解釈させる
	SetConsoleOutputCP(CP_UTF8);

    // 単位行列乗算
    Vector4 v{ 3.0f, 5.0f, 7.0f, 1.0f };
    Vector4 result{ Mat4x4::Mul(v, Mat4x4::Identity) };

    Check(v == result, "単位行列乗算");

    // 平行移動
    Mat4x4 t{ Mat4x4::MakeTranslation(v.ToVec3()) };
    Vector3 p{ t.TransformPoint(Vector3::Zero) };

    Check(p == v.ToVec3(), "平行移動行列");

    // 方向ベクトル平行移動
    p = Vector3(1.0f, 0.0f, 0.0f);
    Mat4x4 dirMat{ Mat4x4::MakeTranslation(v.ToVec3()) };
    p = dirMat.TransformDirection(p);

    Check(p == Vector3(1.0f, 0.0f, 0.0f), "方向ベクトルでの平行移動行列");

    // 回転
    Mat4x4 rotateY{ Mat4x4::MakeRotationY(90.0f * Math::DEG_TO_RAD) };
    Vector3 rotateYVec{ rotateY.TransformPoint(Vector3(1.0f, 0.0f, 0.0f)) };

    Check(rotateYVec == Vector3(0.0f, 0.0f, -1.0f), "90度回転");

    // スケール
    Mat4x4 scaleMat{ Mat4x4::MakeScaling(Vector3(2.0f, 3.0f, 4.0f)) };
    Vector4 scaleVec{ Mat4x4::Mul(Vector4(1.0f, 1.0f, 1.0f, 0.0f), scaleMat) };

    Check(scaleVec.ToVec3() == Vector3(2.0f, 3.0f, 4.0f), "スケール");

    // 行列合成
    Mat4x4 scaleDoubleMat{ Mat4x4::MakeScaling(Vector3(2.0f, 2.0f, 2.0f)) };
    Mat4x4 transMat{ Mat4x4::MakeTranslation(Vector3(10.0f, 0.0f, 0.0f)) };
    Vector4 resultScale{ Vector4::One };
    Vector4 resultTrans{ Vector4::One };

    resultScale = Mat4x4::Mul(resultScale, scaleDoubleMat);
    resultScale = Mat4x4::Mul(resultScale, transMat);

    resultTrans = Mat4x4::Mul(resultTrans, transMat);
    resultTrans = Mat4x4::Mul(resultTrans, scaleDoubleMat);

    Check(resultScale.ToVec3() != resultTrans.ToVec3(), "行列合成");

    // LookAt
    // 
    // ケース1
    Mat4x4 view1 = Mat4x4::MakeLookAt(
        { 0,0,0 }, { 0,0,1 }, { 0,1,0 });
    Vector3 p1 = view1.TransformPoint({ 0, 0, 5 });
    // p1は(0, 0, 5)になるはず
    Check(p1 == Vector3{ 0, 0, 5 }, "LookAt(単位行列)");

    // ケース2
    Mat4x4 view2 = Mat4x4::MakeLookAt(
        { 0,0,-5 }, { 0,0,0 }, { 0,1,0 });
    Vector3 p2 = view2.TransformPoint({ 0, 0, 0 });
    // 原点はカメラの前方5mなので、p2は(0, 0, 5)になるはず
    Check(p2 == Vector3{ 0, 0, 5 }, "LookAt(カメラ後ろ)");

    Vector3 quaTestVec{ 3.0f, 5.0f, 1.0f };
    Quaternion identityQua{ Quaternion::Identity }; // 単位四元数
    Vector3 resultTestVec{ identityQua.RotateVector(quaTestVec) };
    // 単位四元数で掛けても出てくる値は元のベクトル
    Check(quaTestVec == resultTestVec, "単位四元数乗算");

    Quaternion rotateQua{ Quaternion::FromAxisAngle(Vector3::Up, 90.0f * Math::DEG_TO_RAD) };
    Vector3 resultRotaVec{ rotateQua.RotateVector(Vector3{1.0f, 0.0f, 0.0f}) };
    // Vector{1, 0, 0}を90°回転させるとVector{0, 0, 1}になる
    Check(resultRotaVec == Vector3{ 0.0f, 0.0f, -1.0f }, "四元数90°回転");

    Mat4x4 testRotQua{ Quaternion::FromAxisAngle(Vector3::Up, 90.0f * Math::DEG_TO_RAD).ToMat4x4() };
    Mat4x4 testRotMat{ Mat4x4::MakeRotationY(90.0f * Math::DEG_TO_RAD) };
    Vector3 resultMat{ Mat4x4::Mul(Vector4{1.0f, 0.0f, 0.0f, 0.0f}, testRotMat).ToVec3() };
    Vector3 resultQua{ Mat4x4::Mul(Vector4{1.0f, 0.0f, 0.0f, 0.0f}, testRotQua).ToVec3() };

    Check(NearEqualVec3(resultMat, resultQua), "ToMat4x4が正しく働くか");

    Quaternion rotateQua2{ Quaternion::FromAxisAngle(Vector3::Up, 90.0f * Math::DEG_TO_RAD) };
    Quaternion rotateQua3{ Quaternion::FromAxisAngle(Vector3::Up, 90.0f * Math::DEG_TO_RAD) };
    Quaternion total{ rotateQua2 * rotateQua3 };
    Vector3 resultVector{ total.RotateVector(Vector3{1.0f, 0.0f, 0.0f}) };

    // Y軸90°回転を掛けて180°で(-1.0f, 0.0f, 0.0f)になるか
    Check(NearEqualVec3(resultVector, Vector3{ -1.0f, 0.0f, 0.0f }), "Quaternion合成");

    // Slerp
    Quaternion aQua{ Quaternion::FromAxisAngle(Vector3::Up, 10.0f * Math::DEG_TO_RAD) }; // 10°
    Quaternion bQua{ Quaternion::FromAxisAngle(Vector3::Up, 90.0f * Math::DEG_TO_RAD) }; // 90°
    Check(aQua == Quaternion::Slerp(aQua, bQua, 0.0f), "Slerp補完(0.0f)");
    Check(bQua == Quaternion::Slerp(aQua, bQua, 1.0f), "Slerp補完(1.0f)");

    // Slerpの中間値
    // Y軸0度とY軸90度の中間(t=0.5)で
    // (1,0,0)を回転すると、45度回転した位置に来るはず
    // cos45° ≈ 0.7071, sin45° ≈ 0.7071
    // 結果は約(0.7071, 0, 0.7071)
    // Slerpの中間値 — 回転後のベクトルで検証
    Quaternion q0{ Quaternion::FromAxisAngle(Vector3::Up, 0.0f) };
    Quaternion q90{ Quaternion::FromAxisAngle(Vector3::Up, 90.0f * Math::DEG_TO_RAD) };
    Quaternion qMid{ Quaternion::Slerp(q0, q90, 0.5f) };

    Vector3 rotated = qMid.RotateVector(Vector3::Right);  // (1,0,0)を回転
    Check(NearEqual(rotated.x, 0.7071f) && NearEqual(rotated.z, -0.7071f), "Slerp中間値");

	const Quaternion rotation{ Quaternion::FromToRotation(Vector3::Up, Vector3::Right) };
	const Vector3 quaternionResult{ rotation.RotateVector(Vector3::Up) };
	const Vector3 matrixResult{ rotation.ToMat4x4().TransformDirection(Vector3::Up) };
	DEBUG_LOG("Quaternion = ({}, {}, {}) Matrix = ({}, {}, {})\n", quaternionResult.x, quaternionResult.y, quaternionResult.z, matrixResult.x, matrixResult.y, matrixResult.z);

    // 0除算による警告テスト
    Vector2 aVec2{ 10.0f, 10.0f };
    aVec2 /= 0.0f;
    Vector3 aVec3{ 10.0f, 10.0f, 10.0f };
    aVec3 /= 0.0f;
    Vector4 aVec4{ 10.0f, 10.0f, 10.0f, 10.0f };
    aVec4 /= 0.0f;

	// 3D正射影行列(横20,縦10でNear = 1, Far = 101なのでX = -10~10からNDCの-1+1へYは-5~5からNDCの-1~1,ZはDirectXの深度座標0~1へ変換される)
	const Mat4x4 orthographic{ Mat4x4::MakeOrthGraphic(20.0f, 10.0f, 1.0f, 101.0f) };
	const Vector4 orthographicNear{ Mat4x4::Mul(Vector4{-10.0f, -5.0f, 1.0f, 1.0f}, orthographic) }; // 正射影範囲の左下手前
	const Vector4 orthographicFar{ Mat4x4::Mul(Vector4{10.0f, 5.0f, 101.0f, 1.0f}, orthographic) }; // 正射影範囲の右上奥
	const Vector4 orthographicCenter{ Mat4x4::Mul(Vector4{0.0f, 0.0f, 51.0f, 1.0f}, orthographic) }; // 正射影範囲の中心
	// 左下手前チェック
	Check(NearEqual(orthographicNear.x, -1.0f) && NearEqual(orthographicNear.y, -1.0f) && NearEqual(orthographicNear.z, 0.0f) && NearEqual(orthographicNear.w, 1.0f), "3D正射影Near");
	// 右上奥チェック
	Check(NearEqual(orthographicFar.x, 1.0f) && NearEqual(orthographicFar.y, 1.0f) && NearEqual(orthographicFar.z, 1.0f) && NearEqual(orthographicFar.w, 1.0f), "3D正射影行列 Far");
	// 中心チェック
	Check(NearEqual(orthographicCenter.x, 0.0f) && NearEqual(orthographicCenter.y, 0.0f) && NearEqual(orthographicCenter.z, 0.5f) && NearEqual(orthographicCenter.w, 1.0f), "3D正射影行列 Center");

	// 平行光源用Shadow行列
	DirectionalLight shadowLight{};
	// 光源は上空から真下へ
	shadowLight.direction = Vector3::Down;
	DirectionalShadowSettings shadowSettings{};
	// 原点を中心に、横20・縦10の範囲を影として撮影する
	shadowSettings.focusPosition = Vector3::Zero;
	shadowSettings.lightDistance = 50.0f;
	shadowSettings.width = 20.0f;
	shadowSettings.height = 10.0f;
	shadowSettings.nearClip = 0.0f;
	shadowSettings.farClip = 100.0f;

	ShadowSystem shadowSystem{};
	// 更新が成功するか
	const bool shadowMatrixCreated{ shadowSystem.UpdateDirectionalLightMatrices(shadowLight,shadowSettings) };
	Check(shadowMatrixCreated, "平行光源Shadow行列作成");

	// 光源View空間の確認
	// 光源カメラは原点の50上に配置される。原点は光源カメラか見て50進んだ位置なのでView変換後は(0, 0, 50)
	const Vector3 lightViewCenter{ shadowSystem.GetLightViewMatrix().TransformPoint(shadowSettings.focusPosition) };
	Check(NearEqualVec3(lightViewCenter, Vector3{ 0.0f, 0.0f, 50.0f }), "Shadow中心のLightView変換");

	// LightViewProjection後の中心を確認

	// Nearが0,Farが100なので奥行50はNDCの0.5になる　正射影範囲の中心なのでXとYは0
	const Vector3 shadowNdcCenter{ shadowSystem.GetLightViewProjectionMatrix().TransformPoint(shadowSettings.focusPosition) };
	Check(NearEqualVec3(shadowNdcCenter, Vector3{ 0.0f, 0.0f, 0.5f }), "Shadow中心のNDC変換");

	// 正射影の右端を確認

	// Shadow幅は20なので中心から右へ10進んだ位置が右端になるのでNDCのX = 1と一致するか
	const Vector3 shadowRightEdge{ shadowSystem.GetLightViewProjectionMatrix().TransformPoint(Vector3{ 10.0f, 0.0f, 0.0f }) };
	Check(NearEqualVec3(shadowRightEdge, Vector3{ 1.0f, 0.0f, 0.5f }), "Shadow正射影の右端");

    std::cout << "\n" << passCount << "/" << testCount << " tests passed." << std::endl;


    std::variant<int, float> vari{ 3 }; // intの方に代入
    std::cout << vari.index() << std::endl; // 0が返ってくる
    vari = 2.5f; // floatを代入、int, floatに値が入っているためindexは1を返すと予想
    std::cout << vari.index() << std::endl; // 1が返ってくる
    std::cout << std::visit(Printer{}, vari) << std::endl; // floatが入っているのでfloatが出力される
    vari = 7;
    std::cout << std::visit(Printer{}, vari) << std::endl; // intが入っているのでintが出力される

    return (passCount == testCount) ? 0 : 1;
}
