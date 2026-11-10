#pragma once
#include <windows.h>
#include <cstdint>
#include <vector>

struct ComputeDispatchRequest;
struct GraphicsFrameMetrics;
struct GraphicsGPUTimingFrame;
enum class GraphicsPass : std::uint8_t;

// グラフィックに関する関数のInitialize等ユーザーに提供しない部分をまとめた関数
namespace GfxInternal
{
	// 初期化(ウィンドウのタイトルと幅と高さを設定)
	bool Initialize(HWND _hwnd,  int _clientWidth, int _clientHeight, int _virtualWidth, int _virtualHeight);
	// フレームの開始処理
	void BeginFrame();
	// フレームの終了処理
	void EndFrame();
	// 終了処理
	void Finish();

	// 次フレームの開始前に適用する画面サイズを予約する
	void RequestResize(int _width, int _height);

	// リソースのUploadを記録する
	void RecordUpload();

	// GPU待機を行う 待機が完了したらtrue
	bool WaitForGPU();

	// 完成済みの前フレーム描画統計を取得する
	const GraphicsFrameMetrics& GetLastFrameMetrics();
	// 完成済みのGPU時間計測結果を取得する
	const GraphicsGPUTimingFrame& GetLastGPUTimingFrame();
	// VFXの再生管理がこのフレームに積んだCompute実行要求を取得する
	const std::vector<ComputeDispatchRequest>& GetVFXDispatchRequests();

	// 指定PassのGPU時間計測を開始する(Gfx以外のFacadeの計測をTSLibが仲介するために公開する)
	bool BeginGPUTimingPass(GraphicsPass _pass);
	// Beginに成功したPassのGPU時間計測を終了する
	void EndGPUTimingPass(GraphicsPass _pass, bool _isStarted);
	// 前フレームのVFXメッシュ描画の記録にかかったCPU時間(ミリ秒)を取得する
	double GetLastVFXFlushCpuMilliseconds();
}
