#ifndef VFX_PARTICLE_HLSLI
#define VFX_PARTICLE_HLSLI

// 粒子1個の状態(GPUのBufferにだけ存在する CPU側の GPUBufferDesc::elementStride と同じ64バイト)
struct Particle
{
	float3 position; // ワールド位置
	float life; // 残り寿命(秒) 0以下なら死んでいる(空いている粒子)
	float3 velocity; // 速度(m/s)
	float maxLife; // 生まれたときの寿命(フェードの割合を出すために残す)
	float4 color; // 色と不透明度(加算なら強さ)
	float size; // 板の一辺(m)
	float3 padding; // 16バイト境界に合わせる
};

// 整数ハッシュ 粒子番号と種から毎回違うが同じ入力なら同じ32bit値を作る
// GPUにはrand()がないので 乱数はこうして作る(Wang hash)
uint HashUint(uint _value)
{
	_value = (_value ^ 61u) ^ (_value >> 16u);
	_value *= 9u;
	_value = _value ^ (_value >> 4u);
	_value *= 0x27d4eb2du;
	_value = _value ^ (_value >> 15u);
	return _value;
}

// ハッシュ値を 0から1 の float にする(下位24bitを使う)
float HashToFloat01(uint _hash)
{
	return float(_hash & 0x00ffffffu) / 16777216.0f;
}

// 0から1の乱数2つから 単位球の表面上に一様な向きを作る
// 高さ z を -1から1 に一様に取り その高さの円周上に角度で置くと球面上で一様になる
float3 RandomUnitVector(float _u, float _v)
{
	const float z = _u * 2.0f - 1.0f;
	const float angle = _v * 6.28318530718f; // 2π
	const float radius = sqrt(max(0.0f, 1.0f - z * z));
	return float3(radius * cos(angle), z, radius * sin(angle)); // Yを上にする
}

#endif
