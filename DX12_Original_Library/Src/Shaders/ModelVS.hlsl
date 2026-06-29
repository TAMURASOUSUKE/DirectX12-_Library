// テクスチャを表示するための基本的なシェーダー
#pragma pack_matrix(row_major) // 全ての行列を行優先としてあつかう

cbuffer MVP : register(b0) // ルートパラメータ[0]のCBV
{
    float4x4 mvp;
}

cbuffer BoneCB : register(b2)
{
    float4x4 boneMatrices[256]; // スキニング行列
}

struct VS_INPUT
{
    float3 position : POSITION; // 位置
    float3 normal : NORMAL; // 法線
    float2 uv : TEXCOORD; // テクスチャ
    float4 weight : WEIGHTS; // 重み
    uint4 bone : BONES; // ボーン
};

struct VS_OUTPUT
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD;
};

VS_OUTPUT main(VS_INPUT _input)
{
    VS_OUTPUT output;
    
    // 4つのボーン行列を重みで混ぜることで行列を作成する
    float4x4 skinMatrix =
        boneMatrices[_input.bone.x] * _input.weight.x +
        boneMatrices[_input.bone.y] * _input.weight.y +
        boneMatrices[_input.bone.z] * _input.weight.z +
        boneMatrices[_input.bone.w] * _input.weight.w;
    
    // 合成した行列と位置を乗算する
    float4 skinnedPos = mul(float4(_input.position, 1.0f), skinMatrix);

    // MVP乗算
    output.position = mul(skinnedPos, mvp); // skinnedPosにmvpを掛ける
    output.uv = _input.uv;
    return output;
}