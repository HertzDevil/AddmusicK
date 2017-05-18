#pragma once

#include <string_view>
#include <vector>
#include <optional>
#include <algorithm>

namespace AMKd::MML {

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
	void SkipSpaces();

	void Clear();
	void Unput();
	bool PushMacro(std::string_view key, std::string_view repl);
	bool PopMacro();

	std::size_t GetLineNumber() const;
	std::size_t GetReadCount() const;
	void SetReadCount(std::size_t count);
	explicit operator bool() const;

	template <typename Iter>
	bool DoReplacement(Iter b, Iter e) {
		while (true) {
			auto it = std::find_if(b, e, [&] (const auto &x) {
				const std::string &rhs = x.first;
				return std::string_view(sv_.data(), rhs.length()) == rhs;
			});
			if (it == e)
				break;
			if (!PushMacro(it->first, it->second))
				return false;
		}
		return true;
	}

private:
	void SetInitReadCount(std::size_t count);

//	std::string filename_;
	std::string mml_;
	std::string_view sv_;
	std::string_view prev_;
	std::vector<MacroState> macros_;
};

} // namespace AMKd::MML
