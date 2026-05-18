#include <algorithm>
#include <string>
#include "ShaderSystem.h"

// 初期化処理
void ShaderSystem::Initialize(ID3D12Device* _device)
{
	// 作成されたデバイスと結合
	if (_device != nullptr)
	{
		device = _device;
	}
}


// 終了処理
void ShaderSystem::Shutdown()
{
	device.Reset();
}

// Shaderのコンパイル
ComPtr<ID3DBlob> ShaderSystem::Compile(const wchar_t* _filePath, const char* _entryPoint, const char* _target)
{
	HRESULT result{}; // 作成結果を格納するオブジェクト
	ComPtr<ID3DBlob> compiledShader{}; // コンパイルされたシェーダーが格納される
	ComPtr<ID3DBlob> errorBlob{}; // エラーが起こった時の対処用

	UINT compileFlags{ 0 }; // コンパイルする際のオプション

	// デバッグ時はデバッグ用のオプションにする
#ifdef _DEBUG
	compileFlags |= D3DCOMPILE_DEBUG; // デバッグ用のフラグ
	compileFlags |= D3DCOMPILE_SKIP_OPTIMIZATION; // シェーダーの最適化を行わない
#endif // _DEBUG

	// コンパイル処理
	result = D3DCompileFromFile(
		_filePath, // ファイルパス
		nullptr, // マクロオブジェクト
		D3D_COMPILE_STANDARD_FILE_INCLUDE, // インクルードオブジェクト(includeを使えるようにする)
		_entryPoint, // エントリーポイント
		_target, // ターゲット(vs,psなど)
		compileFlags, // コンパイルオプション
		0, // エフェクトコンパイルオプション
		&compiledShader, // 格納するためのポインタのアドレス
		&errorBlob // エラー用のポインタのアドレス
		);

	// 失敗したときの処理
	if (FAILED(result))
	{
		// ファイル名が見当たらない時の処理
		if (result == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
		{
			OutputDebugStringA("ファイルが見つかりません\n");
			OutputDebugStringW(_filePath); // ワイド文字でファイル名を出力しどのファイルが見つからなかったかをわかりやすくする
			OutputDebugStringW(L"\n"); // 改行
			return nullptr;
		}

		// エラー時はerrorBolbにメッセージが入るため取り出す
		if (errorBlob)
		{
			const char* errorMessage{ static_cast<const char*>(errorBlob->GetBufferPointer()) }; // バッファの先頭アドレスを取得し文字列に変換する
			std::string errorStr{}; // エラー文字列格納用
			errorStr.assign(errorMessage, errorBlob->GetBufferSize()); // 先頭から文字列のサイズ分だけ再代入する
			errorStr += "\n"; // 改行
			OutputDebugStringA(errorStr.c_str()); // char型を取り出し出力
		}
		else
		{
			// コンパイルに失敗したメッセージを出力
			OutputDebugStringA("Shaderをコンパイルできませんでした\n");
		}
		return nullptr; // 失敗ならnullを返す
	}

	return compiledShader; // コンパイルされたShaderのオブジェクトを返す
}

