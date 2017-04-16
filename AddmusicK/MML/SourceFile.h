#pragma once

#include <string_view>
#include <optional>

namespace AMKd::MML {

class SourceFile
{
public:
	SourceFile(std::string_view data);

	std::optional<std::string> Trim(std::string_view re);
	int GetLineNumber() const;

private:
//	std::string filename_;
	std::string mml_;
	std::string_view sv_;
};

} // namespace AMKd::MML
