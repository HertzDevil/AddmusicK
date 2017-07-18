#pragma once

#include <string_view>
#include <vector>
#include <map>
#include <optional>

namespace AMKd::MML {

// A SourceView wraps around an MML string with methods for tokenization. It
// also handles replacement macros.
class SourceView
{
	struct MacroState
	{
		std::string_view key;
		std::string_view mml;
		std::size_t charCount;
	};
public:
	SourceView();
	explicit SourceView(std::string_view data);
	SourceView(const char *buf, std::size_t size);
	SourceView(std::string &&data) = delete;

	template <typename F>
	bool TryProcess(F&& f) {
		prev_ = sv_;
		if (std::forward<F>(f)(sv_))
			return true;
		sv_ = prev_;
		return false;
	}
	std::optional<std::string_view> Trim(std::string_view re, bool ignoreCase = false);
	std::optional<std::string_view> TrimUntil(std::string_view re, bool ignoreCase = false);
	bool Trim(char ch);
	bool SkipSpaces(); // true if at least one character is skipped

	void Clear();
	bool IsEmpty() const;
	void Unput();

	void AddMacro(const std::string &key, const std::string &repl);
	bool PushMacro(std::string_view key, std::string_view repl);
	bool PopMacro();

	bool HasNextToken();

	std::size_t GetLineNumber() const;
	std::size_t GetReadCount() const;
	void SetReadCount(std::size_t count);
	explicit operator bool() const;

private:
	bool DoReplacement();
	void SetInitReadCount(std::size_t count);

//	std::string filename_;
	std::string_view mml_;
	std::string_view sv_;
	std::string_view prev_;
	std::vector<MacroState> macros_;

	using compare_t = bool (*)(const std::string &, const std::string &);
	std::map<std::string, std::string, compare_t> repl_;		// // //
};

} // namespace AMKd::MML
