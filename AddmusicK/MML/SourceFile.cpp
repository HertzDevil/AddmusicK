#include "SourceFile.h"
#include <regex>

using namespace AMKd::MML;

SourceFile &SourceFile::operator=(const SourceFile &other) {
	if (this != &other) {
		mml_ = other.mml_;
		sv_ = mml_;
		SetReadCount(other.GetReadCount());
		prev_ = sv_;
	}
	return *this;
}

SourceFile::SourceFile(std::string_view data) :
	mml_(data), sv_(mml_), prev_(sv_)
{
}

std::optional<std::string> SourceFile::Trim(std::string_view re) {
	std::regex regex(re.data());
	std::cmatch match;
	std::optional<std::string> z;

	prev_ = sv_;
	if (std::regex_search(sv_.data(), match, regex, std::regex_constants::match_continuous)) {
		size_t len = match[0].length();
		z = sv_.substr(0, len);
		sv_.remove_prefix(len);
	}

	return z;
}

std::optional<std::string> SourceFile::Trim(char re) {
	std::optional<std::string> z;

	prev_ = sv_;
	if (!sv_.empty() && sv_.front() == re) {
		z = std::string(1, re);
		sv_.remove_prefix(1);
	}

	return z;
}

void SourceFile::SkipSpaces() {
	Trim(R"(\s*)");
}

void SourceFile::Clear() {
	prev_ = sv_;
	sv_.remove_prefix(sv_.size());
}

void SourceFile::Unput() {
	sv_ = prev_;
}

int SourceFile::GetLineNumber() const {
	return std::count(mml_.cbegin(), mml_.cend(), '\n') - std::count(sv_.cbegin(), sv_.cend(), '\n') + 1;
}

int SourceFile::GetReadCount() const {
	return mml_.size() - sv_.size();
}

void SourceFile::SetReadCount(std::size_t x) {
	prev_ = sv_;
	sv_ = mml_;
	sv_.remove_prefix(x);
}

SourceFile::operator bool() const {
	return !sv_.empty();
}
