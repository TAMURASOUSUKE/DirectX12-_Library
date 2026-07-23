#include <iostream>
#include <variant>
#include <string>
#include "TSMath.h"

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

    Check(rotateYVec == Vector3(0.0f, 0.0f, 1.0f), "90度回転");

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
    Check(resultRotaVec == Vector3{ 0.0f, 0.0f, 1.0f }, "四元数90°回転");

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
    Check(NearEqual(rotated.x, 0.7071f) && NearEqual(rotated.z, 0.7071f), "Slerp中間値");

    // 0除算による警告テスト
    Vector2 aVec2{ 10.0f, 10.0f };
    aVec2 /= 0.0f;
    Vector3 aVec3{ 10.0f, 10.0f, 10.0f };
    aVec3 /= 0.0f;
    Vector4 aVec4{ 10.0f, 10.0f, 10.0f, 10.0f };
    aVec4 /= 0.0f;


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