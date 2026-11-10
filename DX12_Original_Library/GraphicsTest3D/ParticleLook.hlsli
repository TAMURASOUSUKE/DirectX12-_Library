#ifndef VFX_PARTICLE_LOOK_HLSLI
#define VFX_PARTICLE_LOOK_HLSLI

// 粒子の見た目のパラメータ SetMaterialParameter(material, 0, ParticleLookParameter) で渡す
// CPU側の VfxParticleTest::ParticleLookParameter と同じ並び(80バイト)
cbuffer ParticleLook : register(b0, space1)
{
	float stretchPerSpeed; // 速度1m/sあたりの尾の伸び 0なら伸ばさない
	float maxStretch; // 伸びの上限(倍)
	float growth; // 寿命の終わりまでに何倍大きくなるか(0なら変わらない)
	float rotationSpeed; // 板の回転速度(rad/s) 0なら回さない
	float glowStrength; // 広い光の強さ
	float coreStrength; // 芯の強さ
	float tailFade; // 尾側の明るさ(1なら均一)
	float fadeInRatio; // 生まれてから濃くなりきるまでのageの割合
	float3 hotColor; // 生まれた直後の色
	float hotEnd; // hotColor から粒子色へ移り終わるage(0から1)
	float3 coldColor; // 消える直前の色
	float fadePower; // 残り寿命をこの乗数で不透明度にする(1で線形 2で急に消える)
	float additive; // 1なら加算用 0ならアルファ合成用の出力
	float3 lookPadding;
};

#endif
