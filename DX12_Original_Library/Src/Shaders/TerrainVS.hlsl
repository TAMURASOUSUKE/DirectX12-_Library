// 受け取る値をまとめる構造体
struct VSData
{
	float3 position : POSITION;
	float2 uv : TEXCOORD;
};

VSData main(VSData input)
{
	// 座標変換はDSで行うため、VSではそのまま渡す
	return input;
}
