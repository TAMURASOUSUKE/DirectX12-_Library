#include "../Src/Facade/TSLib.h"

// エントリーポイント
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	const Vector2 windowSize{ 1280.0f, 720.0f };

	// 初期化 失敗したら-1を返す
	if (!TSLib::Initialize(L"FileTest", static_cast<int>(windowSize.x), static_cast<int>(windowSize.y)))return -1;
	Time::SetTargetFPS(0);


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
