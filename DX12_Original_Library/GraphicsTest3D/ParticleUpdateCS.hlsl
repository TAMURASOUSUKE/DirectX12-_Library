#include "Particle.hlsli"

// 粒子の更新と発生を1つのCSで行う(火花・煙・水を同じCSでパラメータだけ変えて動かす)
// スレッド1本 = 粒子1個 生きていれば動かし 死んでいれば空いている粒子の取り合いに参加して生き返る

// 発生と更新のパラメータ(CPUの ParticleUpdateParameter と同じ並び 128バイト)
cbuffer ParticleUpdateParameter : register(b0)
{
	float3 emitPosition; // 発生位置(ワールド)
	float deltaTime; // 前フレームからの時間(秒)
	float3 gravity; // 加速度(m/s^2) 煙は上向きにして浮力にする
	uint emitCount; // このフレームで生き返らせる数
	uint particleCount; // Bufferの要素数(上限)
	uint randomSeed; // 毎フレーム変える乱数の種
	float lifeMin; // 寿命の下限(秒)
	float lifeMax; // 寿命の上限(秒)
	float speedMin; // 初速の下限(m/s)
	float speedMax; // 初速の上限(m/s)
	float size; // 板の一辺(m)
	float drag; // 空気抵抗(1秒で失う速度の割合)
	float4 color; // 色
	float3 emitVelocity; // 発生源の速度(粒子に引き継いで尾を引かせる)
	float swirl; // 風の場(渦)の強さ 0なら風なし
	float3 coneDirection; // 初速の向きを寄せる方向
	float coneBias; // 寄せる強さ(0なら球面一様)
	float spawnRadius; // 発生位置のばらつき半径
	float groundY; // この高さで跳ね返る(使わないなら十分低い値)
	float bounce; // 跳ね返りで残る速度の割合
	float sizeSpread; // 大きさのばらつき(0.4なら0.6から1.4倍)
};

RWStructuredBuffer<Particle> particles : register(u0); // 粒子の状態(読み書き)
RWStructuredBuffer<uint> emitCounter : register(u1); // 空いている粒子の取り合いカウンタ(毎フレーム0から)

// 位置から風の速度を返す速度場 粒子をこの場に乗せて運ぶのが移流(advection)
// 格子流体では場そのものを毎フレーム計算で求めるが ここでは渦とうねりを式で与えている
float3 WindField(float3 _position)
{
	const float3 relative = _position - emitPosition;
	// 発生源を通る縦軸の周りを回る渦 上方向 × 半径方向 が接線方向 中心から離れるほど弱める
	const float3 tangent = cross(float3(0.0f, 1.0f, 0.0f), relative) / (length(relative.xz) + 1.0f);
	// 高さで向きが変わるうねり(sin/cosで滑らかに変化させる)
	const float3 wobble = float3(sin(_position.y * 0.9f + _position.z * 0.5f), 0.0f, cos(_position.y * 0.7f + _position.x * 0.5f)) * 0.35f;
	return (tangent + wobble) * swirl;
}

[numthreads(256, 1, 1)]
void main(uint3 _id : SV_DispatchThreadID)
{
	const uint index = _id.x;
	if (index >= particleCount) return; // 要素数がグループの倍数でない余りを捨てる

	Particle particle = particles[index];

	if (particle.life > 0.0f)
	{
		// 半陰的オイラー法 先に速度を更新し その速度で位置を進める
		// 位置を先に進める(陽的)より安定し 重力のような一定加速度では十分な精度
		particle.velocity += gravity * deltaTime;

		// 風の場に乗せる 場の速度との差を少しずつ縮める(空気に引きずられる近似)
		if (swirl > 0.0f)
		{
			particle.velocity += (WindField(particle.position) - particle.velocity) * saturate(1.5f * deltaTime);
		}

		// 空気抵抗の近似 速度に比例して減速する(1秒で drag の割合を失う)
		particle.velocity *= max(0.0f, 1.0f - drag * deltaTime);
		particle.position += particle.velocity * deltaTime;

		// 地面との衝突 めり込みを戻し 縦速度を反転して弱め 横も少し失う
		if (particle.position.y < groundY)
		{
			particle.position.y = groundY;
			particle.velocity.y = -particle.velocity.y * bounce;
			particle.velocity.xz *= 0.7f;
		}

		particle.life -= deltaTime;
	}
	else
	{
		// 空いている粒子の取り合い カウンタを1増やし 増やす前の値が emitCount 未満なら自分が生き返る
		// InterlockedAdd は複数スレッドが同時に呼んでも1回ずつ順に足されることが保証された命令
		uint slot;
		InterlockedAdd(emitCounter[0], 1u, slot);
		if (slot < emitCount)
		{
			// 粒子番号と種を混ぜて この粒子だけの乱数を作る
			const uint hashBase = HashUint(index * 1664525u + randomSeed);
			const float random0 = HashToFloat01(hashBase);
			const float random1 = HashToFloat01(HashUint(hashBase + 1u));
			const float random2 = HashToFloat01(HashUint(hashBase + 2u));
			const float random3 = HashToFloat01(HashUint(hashBase + 3u));
			const float random4 = HashToFloat01(HashUint(hashBase + 4u));
			const float random5 = HashToFloat01(HashUint(hashBase + 5u));
			const float random6 = HashToFloat01(HashUint(hashBase + 6u));
			const float random7 = HashToFloat01(HashUint(hashBase + 7u));

			// 向きは球面上で一様に取り coneDirection へ coneBias ぶん寄せる(0なら全方向)
			float3 direction = RandomUnitVector(random0, random1);
			direction = normalize(direction + coneDirection * coneBias);

			// lerp(a, b, t) = a + (b - a) * t 0から1の乱数を範囲へ写す
			const float speed = lerp(speedMin, speedMax, random2);
			const float life = lerp(lifeMin, lifeMax, random3);

			// 発生位置は球の中にばらまく(半径にも乱数を掛けて中心寄りにする)
			particle.position = emitPosition + RandomUnitVector(random5, random6) * (spawnRadius * random7);
			particle.velocity = direction * speed + emitVelocity; // 発生源が動いていればその速度も乗る
			particle.life = life;
			particle.maxLife = life;
			particle.color = color;
			particle.size = size * lerp(1.0f - sizeSpread, 1.0f + sizeSpread, random4);
		}
	}

	particles[index] = particle;
}
