#pragma once

#include <string_view>
#include <optional>

namespace AMKd::MML {

class SourceFile
{
public:
	SourceFile() = default;
	~SourceFile() = default;
	SourceFile(const SourceFile &) = default;
	SourceFile(SourceFile &&) noexcept = default;
	SourceFile &operator=(const SourceFile &other);
	SourceFile &operator=(SourceFile &&) = default;

	explicit SourceFile(std::string_view data);
	
	std::optional<std::string> Trim(std::string_view re);
	std::optional<std::string> Trim(char re);
	void SkipSpaces();

	void Clear();
	void Unput();
	int GetLineNumber() const;

	int GetReadCount() const;
	void SetReadCount(std::size_t x);
	explicit operator bool() const;

private:
//	std::string filename_;
	std::string mml_;
	std::string_view sv_;
	std::string_view prev_;
};

} // namespace AMKd::MML
