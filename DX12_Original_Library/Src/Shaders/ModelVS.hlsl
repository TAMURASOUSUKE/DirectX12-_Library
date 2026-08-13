// テクスチャを表示するための基本的なシェーダー
#pragma pack_matrix(row_major) // 全ての行列を行優先としてあつかう

// 1フレーム共通
cbuffer SceneFrameCB : register(b0)
{
	float4x4 viewProjection;

    // xyz：カメラ位置 w：未使用
	float4 cameraPosition;
}

// モデル個体ごと
cbuffer ModelObjectCB : register(b4)
{
	float4x4 world;
	float4x4 worldInverseTranspose;
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
	float3 worldNormal : NORMAL0;
	float3 worldPosition : POSITION0;
};

VS_OUTPUT main(VS_INPUT _input)
{
    VS_OUTPUT output;
    
	float4 pos = float4(_input.position, 1.0f);
    // 頂点が影響を受ける4本のボーン行列を作成して位置と乗算
	const float4 skinnedPos =
        mul(pos, boneMatrices[_input.bone.x]) * _input.weight.x +
		mul(pos, boneMatrices[_input.bone.y]) * _input.weight.y +
		mul(pos, boneMatrices[_input.bone.z]) * _input.weight.z +
        mul(pos, boneMatrices[_input.bone.w]) * _input.weight.w;
	
	// スキニング後のローカル座標をワールド空間へ移す
	const float4 worldPosition = mul(skinnedPos, world);
	
	// 法線は位置ではなく方向のためw = 0
	const float4 localNormal = float4(_input.normal, 0.0f);
	// 法線も同じ4本のボーンに追従させる
	const float3 skinnedNormal = 
		mul(localNormal, boneMatrices[_input.bone.x]).xyz * _input.weight.x +
		mul(localNormal, boneMatrices[_input.bone.y]).xyz * _input.weight.y +
		mul(localNormal, boneMatrices[_input.bone.z]).xyz * _input.weight.z +
        mul(localNormal, boneMatrices[_input.bone.w]).xyz * _input.weight.w;
	
	// 画面座標へ変換
	output.position = mul(worldPosition, viewProjection);
	// PBRで使用するためにワールド座標を残す
	output.worldPosition = worldPosition.xyz;
	// モデルの回転と非均一スケールを法線へ反映
	output.worldNormal = normalize(mul(float4(skinnedNormal, 0.0f), worldInverseTranspose).xyz);
    output.uv = _input.uv;
    return output;
}
