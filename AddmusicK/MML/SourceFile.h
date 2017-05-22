#pragma once

#include <string_view>
#include <vector>
#include <map>
#include <optional>
#include <algorithm>
#include "../Utility/Trie.h"

namespace AMKd::MML {

// // //
const auto replComp = [] (const std::string &a, const std::string &b) {
	size_t al = a.length(), bl = b.length();
	return std::tie(al, b) > std::tie(bl, a);
};

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
	template <typename T, typename U>
	std::optional<T> ExtractToken(const AMKd::Utility::Trie<T, U> &cmds) {
		prev_ = sv_;
		if (auto result = cmds.SearchValue(sv_))
			return *result;
		sv_ = prev_;
		return std::nullopt;
	}

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

	std::map<std::string, std::string, decltype(replComp)> repl_ {replComp};		// // //
};

} // namespace AMKd::MML
