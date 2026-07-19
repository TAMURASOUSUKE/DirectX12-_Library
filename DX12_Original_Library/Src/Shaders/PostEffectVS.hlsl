
// ポストエフェクトを行う際の三角形を生成する
struct VSOutput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

// 頂点バッファを使わずにSV_VertexIDの0,1,2から三角形を生成する
VSOutput main(uint _vertexID : SV_VertexID)
{
    VSOutput output;
    
    // VertexIDから次のuvを生成する
    // 0 : (0, 0)
    // 1 : (2, 0)
    // 2 : (0, 2)
    float2 uv = float2((_vertexID << 1) & 2, _vertexID & 2);
    output = uv;
    // UVをNDC座標へ変換する
    // (-1,  1)
	// ( 3,  1)
	// (-1, -3)
    // 画面全体より大きな三角形を1枚描き、画面全体を確実に覆う
    output.position = float4(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f, 0.0f, 1.0f);
    return output;
}