#include "../Src/Facade/TSLib.h"

namespace {
	// テスト結果を統一した形式でログへ出す
	bool CheckTest(bool _condition, const char* _testName)
	{
		if (_condition)
		{
			DEBUG_LOG("[PASS] {}\n", _testName);
			return true;
		}

		DEBUG_LOG_ERROR("[FAIL] {}\n", _testName);
		return false;
	}

	// 暗号化ファイル機能をまとめて検証する
	bool RunEncryptedFileTests()
	{
		const char* encryptedPath{ "Res/EncryptedTest.bin" };
		const char* secondEncryptedPath{ "Res/EncryptedTest02.bin" };
		const char* emptyEncryptedPath{ "Res/EncryptedEmptyTest.bin" };

		// テスト用の正しい鍵
		File::CryptoKey correctKey{};
		for (std::size_t i = 0; i < correctKey.bytes.size(); ++i)
		{
			correctKey.bytes[i] = static_cast<std::uint8_t>(i + 1);
		}

		// 正しい鍵とは異なるテスト用の鍵
		File::CryptoKey wrongKey{};
		for (std::size_t i = 0; i < wrongKey.bytes.size(); ++i)
		{
			wrongKey.bytes[i] = static_cast<std::uint8_t>(0xFF - i);
		}

		// 0x00や0xFFを含め、文字列だけではないデータを検証する
		const std::vector<std::uint8_t> sourceBytes{
			0x54, 0x53, 0x47, 0x61, 0x6D, 0x65,
			0x4C, 0x69, 0x62, 0x00, 0x80, 0xFF
		};

		bool allPassed{ true };
		// 正しい鍵で暗号化と復号ができるか
		const bool writeSucceeded{ File::WriteEncryptedBytes(encryptedPath, sourceBytes, correctKey) };
		allPassed &= CheckTest(writeSucceeded, "暗号化ファイルを書き込める");

		std::vector<std::uint8_t> decryptedBytes{};

		const bool readSucceeded{ File::ReadEncryptedBytes(encryptedPath, correctKey, decryptedBytes) };
		allPassed &= CheckTest(readSucceeded, "正しい鍵で復号できる");
		allPassed &=CheckTest(decryptedBytes == sourceBytes, "復号後のデータが元データと一致する");

		// 間違った鍵を拒否できるか
		// 失敗時に出力引数が変更されないことも同時に確認する
		const std::vector<std::uint8_t> sentinel{ 0xDE, 0xAD, 0xBE, 0xEF };

		std::vector<std::uint8_t> wrongKeyOutput{ sentinel };

		const bool wrongKeyResult{ File::ReadEncryptedBytes(encryptedPath, wrongKey, wrongKeyOutput) };
		allPassed &= CheckTest(!wrongKeyResult, "間違った鍵による復号を拒否する");
		allPassed &= CheckTest(wrongKeyOutput == sentinel, "復号失敗時に出力データを変更しない");

		// 暗号文の改ざんを検出できるか
		std::vector<std::uint8_t> originalEncryptedFile{};

		const bool encryptedFileReadSucceeded{ File::ReadAllBytes(encryptedPath, originalEncryptedFile) };
		allPassed &= CheckTest(encryptedFileReadSucceeded, "暗号化済みファイルをテスト用に読み込める");

		if (encryptedFileReadSucceeded && !originalEncryptedFile.empty())
		{
			std::vector<std::uint8_t> tamperedFile{ originalEncryptedFile };

			// 最後の1bitだけを反転させる
			// 元データが空でないので、ここは暗号文の領域になる
			tamperedFile.back() ^= 0x01;

			const bool tamperedWriteSucceeded{ File::WriteAllBytes(encryptedPath, tamperedFile) };
			allPassed &= CheckTest(tamperedWriteSucceeded, "改ざんテスト用ファイルを書き込める");

			std::vector<std::uint8_t> tamperedOutput{ sentinel };

			const bool tamperedReadResult{ File::ReadEncryptedBytes(encryptedPath, correctKey, tamperedOutput) };
			allPassed &= CheckTest(!tamperedReadResult, "暗号文の1bit改ざんを検出する");
			allPassed &= CheckTest(tamperedOutput == sentinel, "改ざん検出時に出力データを変更しない");

			// 後続テストのため正常なファイルへ戻す
			const bool restoreSucceeded{ File::WriteAllBytes(encryptedPath, originalEncryptedFile) };
			allPassed &= CheckTest(restoreSucceeded, "改ざん後に元のファイルを復元できる");
		}
		//  同じ平文と鍵でも毎回違う暗号文になるか
		const bool secondWriteSucceeded{ File::WriteEncryptedBytes(secondEncryptedPath, sourceBytes, correctKey) };

		std::vector<std::uint8_t> firstEncryptedBytes{};
		std::vector<std::uint8_t> secondEncryptedBytes{};

		const bool firstFileRead{ File::ReadAllBytes(encryptedPath, firstEncryptedBytes) };
		const bool secondFileRead{ File::ReadAllBytes(secondEncryptedPath, secondEncryptedBytes) };
		allPassed &= CheckTest(secondWriteSucceeded && firstFileRead && secondFileRead, "Nonce確認用の暗号ファイルを用意できる");
		allPassed &= CheckTest(firstEncryptedBytes != secondEncryptedBytes, "同じ平文と鍵でもNonceにより暗号ファイルが異なる");

		// 両方とも同じ平文へ戻ることも確認する
		std::vector<std::uint8_t> secondDecryptedBytes{};
		const bool secondDecryptSucceeded{ File::ReadEncryptedBytes(secondEncryptedPath, correctKey, secondDecryptedBytes) };
		allPassed &= CheckTest(secondDecryptSucceeded && secondDecryptedBytes == sourceBytes, "異なるNonceのファイルも正しく復号できる");

		// 空データも暗号化・復号できるか
		const std::vector<std::uint8_t> emptySource{};
		std::vector<std::uint8_t> emptyOutput{ 0x01 };

		const bool emptyWriteSucceeded{ File::WriteEncryptedBytes(emptyEncryptedPath, emptySource, correctKey) };
		const bool emptyReadSucceeded{ emptyWriteSucceeded && File::ReadEncryptedBytes(emptyEncryptedPath, correctKey, emptyOutput) };
		allPassed &= CheckTest(emptyReadSucceeded, "空データを暗号化して復号できる");
		allPassed &= CheckTest(emptyOutput.empty(), "空データの復号結果が空になる");

		// 最終結果
		if (allPassed)
		{
			DEBUG_LOG("[PASS] 暗号化ファイルテストはすべて成功しました\n");
		}
		else
		{
			DEBUG_LOG_ERROR("[FAIL] 暗号化ファイルテストに失敗しました\n");
		}
		return allPassed;
	}
}

// エントリーポイント
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	const Vector2 windowSize{ 1280.0f, 720.0f };

	// 初期化 失敗したら-1を返す
	if (!TSLib::Initialize(L"FileTest", static_cast<int>(windowSize.x), static_cast<int>(windowSize.y)))return -1;
	Time::SetTargetFPS(0);

	// 暗号化ファイル機能のテスト
	RunEncryptedFileTests();

	// 外部ファイルの読み込み
	// バイナリ
	std::vector<std::uint8_t> bytes{};
	if (File::ReadAllBytes("Res/JapaneseTest.bin", bytes))
	{
		DEBUG_LOG("読込みサイズ : {} byte\n", bytes.size());

		for (const std::uint8_t value : bytes)
		{
			DEBUG_LOG("0x{:02X}\n", value);
		}
	}

	// テキスト
	std::string text{};
	if (File::ReadAllText("Res/日本語テキスト.txt", text))
	{
		DEBUG_LOG("テキスト読込み成功\n{}\n", text);
	}

	// 外部ファイルの書き込み
	const std::vector<std::uint8_t> sourceBytes{ 0x54, 0x53, 0x4C, 0x69, 0x62, 0x00, 0xFF };

	if (!File::WriteAllBytes("Res/日本語書込みテスト.bin", sourceBytes))
	{
		DEBUG_LOG_ERROR("書込みテストに失敗しました\n");
	}

	std::vector<std::uint8_t> loadedBytes{};
	if (File::ReadAllBytes("Res/日本語書込みテスト.bin", loadedBytes))
	{
		if (sourceBytes == loadedBytes)
		{
			DEBUG_LOG("[PASS] 書込み前と読込み後が一致しました\n");
		}
		else
		{
			DEBUG_LOG_ERROR("[FAIL] 書込み前と読込み後が一致しません\n");
		}
	}

	const std::string sourceText{ "TSGameLib 日本語書き込みテスト\nUTF-8で保存されています\n" };
	if (!File::WriteAllText("Res/日本語書込みテスト.txt", sourceText))
	{
		DEBUG_LOG_ERROR("[FAIL] テキストの書込みに失敗しました\n");
	}
	else
	{
		std::string loadedText{};
		if (!File::ReadAllText("Res/日本語書込みテスト.txt", loadedText))
		{
			DEBUG_LOG_ERROR("[FAIL] テキストの読込みに失敗しました\n");
		}
		else if (sourceText == loadedText)
		{
			DEBUG_LOG("[PASS] 書込み前と読込み後のテキストが一致しました\n");
		}
		else
		{
			DEBUG_LOG_ERROR("[FAIL] 書込み前と読込み後のテキストが一致しません\n");
		}
	}

	// Excelを読めるようにするためにBOMを付ける
	const std::string csvText{
	"\xEF\xBB\xBF"
	"Month,StageName,Description\r\n"
	"1,錦帯橋,\"春,桜の季節\"\r\n"
	"2,錦川,\"\"\"鵜飼\"\"の季節\"\r\n"
	"3,紅葉谷,\"1行目\n2行目\"\r\n"
	};

	File::WriteAllText("Res/StageTest.csv", csvText);

	CSVTable table{};

	if (File::LoadCSV("Res/StageTest.csv", table))
	{
		const int monthColumn{ table.FindColumn("Month") };
		const int nameColumn{ table.FindColumn("StageName") };
		const int descriptionColumn{ table.FindColumn("Description") };

		for (const auto& row : table.rows)
		{
			DEBUG_LOG("Month={} Stage={} Description={}\n", row[monthColumn], row[nameColumn], row[descriptionColumn]);
		}
	}
	else
	{
		DEBUG_LOG_ERROR("[FAIL] CSVの読込みに失敗しました\n");
	}

	// ゲームループ
	while (TSLib::ProcessMessage() && !Input::IsKeyPushed(KeyCode::Button::ESC))
	{
		TSLib::BeginFrame(); // フレーム開始処理

		Gfx::ClearScreen(); // 画面クリア(黒)

		TSLib::EndFrame(); // フレーム終了処理
	}

	TSLib::Finish(); // 終了
	return 0;
}
