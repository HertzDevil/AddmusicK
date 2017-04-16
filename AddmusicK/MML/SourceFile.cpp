#include "SourceFile.h"
#include <regex>
#include "../globals.h"

using namespace AMKd::MML;

SourceFile::SourceFile(std::string_view data) : mml_(data), sv_(mml_) {
}

std::optional<std::string> SourceFile::Trim(std::string_view re) {
	std::regex regex(re.data());
	std::cmatch match;
	std::optional<std::string> z;

	if (std::regex_search(sv_.data(), match, regex, std::regex_constants::match_continuous)) {
		size_t len = match[0].length();
		z = sv_.substr(0, len);
		sv_.remove_prefix(len);
	}

	return z;
}

int SourceFile::GetLineNumber() const {
	return std::count(mml_.cbegin(), mml_.cend(), '\n') - std::count(sv_.cbegin(), sv_.cend(), '\n') + 1;
}
