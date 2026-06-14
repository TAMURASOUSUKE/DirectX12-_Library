#pragma once
#include <format>
#include <string>
#include <utility>
// デバッグに使えるマクロの内部を実装する

// 内部実装を行う(ユーザーからこのnameSpaceは見えてしまうが公開マクロは呼び出し地点で展開されるので容認する。代わりに名前で触らないようにする)
namespace Debug::Detail
{
	// 実際に出力する時に使われる関数
	void Write(const std::string& _msg);

	/// <summary>
	/// Assert : 失敗処理
	/// </summary>
	/// <param name="_cond">条件式</param>
	/// <param name="_expr">文字列化された条件式</param>
	/// <param name="_file">ファイル名</param>
	/// <param name="_line">位置</param>
	void AssertImpl(bool _cond, const char* _expr, const char* _file, int _line);

	/// <summary>
	/// Logの実体(あくまでformatへの中継関数のため受け取った値の性質を変えないためにforwardをする)
	/// </summary>
	/// <typeparam name="Args">可変長引数の型</typeparam>
	/// <param name="_level">Logの種類</param>
	/// <param name="_file">ファイル名</param>
	/// <param name="_line">位置</param>
	/// <param name="_fmt">出力文</param>
	/// <param name="_args">出力文内に埋め込む値</param>
	template <class... Args>
	void LogImpl(const char* _level, const char* _file, int _line,
				 std::format_string<Args...> _fmt, Args&&... _args)
	{
		// _args内に入った右辺値が左辺値として解釈されないようにforwardを付ける(関数内では名前付き引数は左辺値扱いされるため)
		std::string body{ std::format(_fmt, std::forward<Args>(_args)...) }; // 出力する文に変換
		Write(std::format("{} {} : {} {}\n", _level, _file, _line, body));  // 出力する
	}
}

#ifdef _DEBUG
	#define DEBUG_LOG(fmt, ...) Debug::Detail::LogImpl("[Info]", __FILE__, __LINE__, fmt __VA_OPT__(,) __VA_ARGS__)
	#define DEBUG_LOG_WARNING(fmt, ...) Debug::Detail::LogImpl("[Warning]", __FILE__, __LINE__, fmt __VA_OPT__(,) __VA_ARGS__)
	#define DEBUG_LOG_ERROR(fmt, ...) Debug::Detail::LogImpl("[Error]", __FILE__, __LINE__, fmt __VA_OPT__(,) __VA_ARGS__)
	#define DEBUG_ASSERT(cond) Debug::Detail::AssertImpl((cond), #cond, __FILE__, __LINE__)
#else
// Release時には動かない
	#define DEBUG_LOG(fmt, ...) ((void)0)
	#define DEBUG_LOG_WARNING(fmt, ...) ((void)0)
	#define DEBUG_LOG_ERROR(fmt, ...) ((void)0)
	#define DEBUG_ASSERT(cond) ((void)0)
#endif // _DEBUG