#pragma once

#include <string_view>
#include <vector>
#include <map>
#include <optional>

namespace AMKd::MML {

namespace Lexer {
struct Tokenizer;
}

// A SourceFile wraps around an MML string with methods for tokenization. It
// also handles replacement macros.
class SourceFile
{
	struct MacroState
	{
		std::string_view key;
		std::string mml;
		std::size_t charCount;
	};
public:
	SourceFile();
	explicit SourceFile(std::string_view data);
	~SourceFile() = default;
	SourceFile(const SourceFile &);
	SourceFile(SourceFile &&) noexcept;
	SourceFile &operator=(const SourceFile &other);
	SourceFile &operator=(SourceFile &&other);

	std::optional<std::string> Trim(std::string_view re, bool ignoreCase = false);
	std::optional<std::string> Trim(char re);
	int Peek() const;		// // //
	bool SkipSpaces(); // true if at least one character is skipped

	void Clear();
	bool IsEmpty() const;
	void Unput();

	void AddMacro(const std::string &key, const std::string &repl);
	bool PushMacro(std::string_view key, std::string_view repl);
	bool PopMacro();

	bool HasNextToken();
	friend struct Lexer::Tokenizer;

	std::size_t GetLineNumber() const;
	std::size_t GetReadCount() const;
	void SetReadCount(std::size_t count);
	explicit operator bool() const;

private:
	bool DoReplacement();
	void SetInitReadCount(std::size_t count);

//	std::string filename_;
	std::string mml_;
	std::string_view sv_;
	std::string_view prev_;
	std::vector<MacroState> macros_;

	using compare_t = bool (*)(const std::string &, const std::string &);
	std::map<std::string, std::string, compare_t> repl_;		// // //
};

} // namespace AMKd::MML
