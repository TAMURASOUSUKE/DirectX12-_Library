#pragma once
struct ID3D12GraphicsCommandList; // ポインタでの保持のみなので前方宣言で事足りる

// ここではRenderDocのデバッグの際に何がドローコールしたのか等を明確にわかるようにするために扱う関数を定義する
class GPUMarker
{
public:
	// 引数で名前を決定する(暗黙的型変換の禁止)
	explicit GPUMarker(const char* _name);
	// デストラクタ(ここで閉じる)
	~GPUMarker();

private:
	ID3D12GraphicsCommandList* cmdList{};
};

#ifdef _DEBUG
// __LINE__を展開するためのに段階分け
#define GPU_MARKER_CONCAT_(a, b) a##b // トークン結合
#define GPU_MARKER_CONCAT(a, b) GPU_MARKER_CONCAT_(a, b)
#define GPU_MARKER(name) GPUMarker GPU_MARKER_CONCAT(gpuMarker, __LINE__)(name)
#else
#define GPU_MARKER(name)((void)0)
#endif // _DEBUG
