#pragma pack_matrix(row_major)
#include "../Src/Shaders/VFXContract.hlsli"
#include "Particle.hlsli"
#include "ParticleLook.hlsli"

// 粒子のVS 頂点バッファを使わず SV_InstanceID で粒子を SV_VertexID で板の隅を決める
// 描画は DrawInstanced(4, 粒子数) で 1粒子 = 4頂点のTRIANGLESTRIP
// 見た目の違い(尾の長さ 成長 回転 色の変化)は ParticleLook(SetMaterialParameter slot0)で与える

// SetMaterialBuffer(slot0) で結んだ粒子Buffer(読み取り専用)
StructuredBuffer<Particle> particles : register(t0, space1);

VFXVertexOutput main(uint _vertexID : SV_VertexID, uint _instanceID : SV_InstanceID)
{
	const Particle particle = particles[_instanceID];
	VFXVertexOutput output;

	// 死んでいる粒子は大きさ0にして 三角形がつぶれて画素にならないようにする
	const bool alive = particle.life > 0.0f;
	const float lifeRatio = alive ? saturate(particle.life / particle.maxLife) : 0.0f;
	const float age = 1.0f - lifeRatio; // 0 = 生まれた直後 1 = 消える直前

	// ageで大きくなる(煙用) growth = 1 なら寿命の終わりに2倍
	const float halfSize = alive ? particle.size * 0.5f * (1.0f + growth * age) : 0.0f;

	// 頂点番号 0から3 を板の隅 (-1,+1)(+1,+1)(-1,-1)(+1,-1) に写す
	// bit0 で左右 bit1 で上下 並びは VFX03 の DrawQuad3D と同じ左上 右上 左下 右下
	const float baseX = (_vertexID & 1u) ? 1.0f : -1.0f;
	const float baseY = (_vertexID & 2u) ? -1.0f : 1.0f;
	float cornerX = baseX;
	float cornerY = baseY;

	// 板の回転 隅の座標を2D回転行列で回す 粒子ごとに初期角を変え ageで回し続ける
	if (rotationSpeed != 0.0f)
	{
		const float angle = HashToFloat01(HashUint(_instanceID)) * 6.28318530718f + rotationSpeed * (particle.maxLife - particle.life);
		const float s = sin(angle);
		const float c = cos(angle);
		cornerX = baseX * c - baseY * s;
		cornerY = baseX * s + baseY * c;
	}

	// カメラの視線方向 = 右 × 上(左手系 Z = X Cross Y) 板はこの方向に垂直な面に乗せる
	const float3 cameraForward = normalize(cross(cameraRight.xyz, cameraUp.xyz));

	// 速度から視線方向の成分を引く(射影)と板の面に乗った向きになる これを長辺にする
	const float speed = length(particle.velocity);
	float3 longAxis = particle.velocity - cameraForward * dot(particle.velocity, cameraForward);
	const float longLength = length(longAxis);
	const bool useStretch = (stretchPerSpeed > 0.0f) && (longLength > 1e-4f);
	longAxis = useStretch ? longAxis / longLength : cameraRight.xyz; // 伸ばさない時はカメラの右・上をそのまま使う
	const float3 shortAxis = useStretch ? normalize(cross(cameraForward, longAxis)) : cameraUp.xyz;

	// 速いほど長い尾 頭(cornerX = +1)を粒子の位置に置き 尾は進行方向の後ろへ伸ばす
	const float stretch = useStretch ? 1.0f + min(speed * stretchPerSpeed, maxStretch) : 1.0f;
	const float halfLong = halfSize * stretch;
	const float3 center = particle.position - longAxis * (halfLong - halfSize);
	const float3 worldPosition = center + longAxis * (cornerX * halfLong) + shortAxis * (cornerY * halfSize);
	output.position = mul(float4(worldPosition, 1.0f), viewProjection);

	// UVは回転前の隅から作る(板ごと回るので模様も一緒に回る)
	output.uv = float2(baseX * 0.5f + 0.5f, 0.5f - baseY * 0.5f); // 左上が(0,0) u=0 が尾 u=1 が頭

	// ageで色を変える hotColor → 粒子色 → coldColor hotEnd が切り替わるage
	const float3 rgb = (age < hotEnd)
		? lerp(hotColor, particle.color.rgb, age / max(hotEnd, 1e-4f))
		: lerp(particle.color.rgb, coldColor, (age - hotEnd) / max(1.0f - hotEnd, 1e-4f));

	// 生まれた直後は fadeInRatio の間に濃くなり 残り寿命の fadePower 乗で消える
	const float fadeIn = saturate(age / max(fadeInRatio, 1e-4f));
	output.color = float4(rgb, particle.color.a * fadeIn * pow(lifeRatio, fadePower));
	return output;
}
