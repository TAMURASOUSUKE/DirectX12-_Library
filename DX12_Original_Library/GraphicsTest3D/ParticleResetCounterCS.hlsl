// 空いている粒子の取り合いに使うカウンタを毎フレーム0に戻す(1スレッドだけ)
// ParticleUpdateCS より先に Dispatch する(同じBufferへの書き込み順は ComputeSystem が UAV Barrier で守る)
RWStructuredBuffer<uint> emitCounter : register(u0);

[numthreads(1, 1, 1)]
void main(uint3 _id : SV_DispatchThreadID)
{
	emitCounter[0] = 0;
}
