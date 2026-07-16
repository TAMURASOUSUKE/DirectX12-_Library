#pragma once
#include <windows.h>
#include <d3d12.h>
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;

// 定数バッファをリングバッファとして管理して定数バッファに書き込み中に切り替えが行われないようにする
class RingConstantBuffer
{
public:
	// 初期化
	void Initialize(UINT _dataSize);
	// 終了処理
	void Shutdown();
	// 更新(書き込み先,読み込み元,データサイズ) そのスライスのGPUアドレスを返す
	D3D12_GPU_VIRTUAL_ADDRESS Update(const void* _src, UINT _size);
	// GPUアドレスを返す(オフセットを計算した状態)
	D3D12_GPU_VIRTUAL_ADDRESS GetCurrentVirtualAddress() const;
	// フレームの頭で読んでカウンターをリセットする
	void Reset();
private:
	// Offsetを計算するヘルパー関数
	UINT CalculateOffset() const;

private:
	ComPtr<ID3D12Resource> resource; // リソース本体
	void* baseCPUPtr{ nullptr }; // マップした時のベースとなるCPUアドレス
	D3D12_GPU_VIRTUAL_ADDRESS baseGPUVA{ 0 }; // ベースとなるGPUの仮想アドレス
	UINT alignedSize{ 0 }; // スライス幅(256境界対応)
	UINT frameCounter{ 0 }; // そのフレームでどれだけ呼ばれたか

};