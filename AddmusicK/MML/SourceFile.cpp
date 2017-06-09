#include "SourceFile.h"
#include "../Utility/Exception.h"
#include <regex>
#include <unordered_map>

namespace {
	bool replComp(const std::string &a, const std::string &b) {
		size_t al = a.length(), bl = b.length();
		return std::tie(al, b) > std::tie(bl, a);
	}

	const std::regex &get_re(std::string_view re, bool ignoreCase) {
		static std::unordered_map<std::string_view, std::regex> regex_cache;
		auto flag = std::regex::ECMAScript | std::regex::optimize | (ignoreCase ? std::regex::icase : (std::regex_constants::syntax_option_type)0);

		auto it = regex_cache.find(re);
		if (it != regex_cache.cend())
			return it->second;
		std::regex cache {re.data(), flag};
		auto result = regex_cache.insert(std::make_pair(re, cache));
		return result.first->second;
	}
}

using namespace AMKd::MML;

SourceFile::SourceFile() : sv_(mml_), prev_(sv_), repl_(replComp)
{
	Trim("\xEF\xBB\xBF"); // utf-8 bom
}

SourceFile::SourceFile(std::string_view data) :
	mml_(data), sv_(mml_), prev_(sv_), repl_(replComp)
{
}

SourceFile::SourceFile(const SourceFile &other) :
	mml_(other.mml_), repl_(replComp)
{
	SetInitReadCount(other.GetReadCount());
}

SourceFile::SourceFile(SourceFile &&other) noexcept :
	mml_(std::move(other.mml_)), repl_(replComp)
{
	SetInitReadCount(mml_.size() - other.sv_.size());
}

SourceFile &SourceFile::operator=(const SourceFile &other) {
	mml_ = other.mml_;
	SetInitReadCount(other.GetReadCount());
	return *this;
}

SourceFile &SourceFile::operator=(SourceFile &&other) {
	size_t len = other.GetReadCount();
	mml_.swap(other.mml_);
	SetInitReadCount(len);
	return *this;
}

std::optional<std::string> SourceFile::Trim(std::string_view re, bool ignoreCase) {
	std::cmatch match;
	std::optional<std::string> z;

	prev_ = sv_;
	if (std::regex_search(sv_.data(), match, get_re(re, ignoreCase),
						  std::regex_constants::match_continuous)) {
		size_t len = match[0].length();
		z = sv_.substr(0, len);
		sv_.remove_prefix(len);
	}

	return z;
}

bool SourceFile::Trim(char re) {
	prev_ = sv_;
	if (!sv_.empty() && sv_.front() == re) {
		sv_.remove_prefix(1);
		return true;
	}

	return false;
}

int SourceFile::Peek() const {
	if (sv_.empty())
		return -1;
	return sv_.front();
}

bool SourceFile::SkipSpaces() {
	bool ret = false;
	do {
		ret = ret || !Trim(R"(\s*)")->empty();
	} while (sv_.empty() && PopMacro());
	return ret;
}

void SourceFile::Clear() {
	if (!macros_.empty())
		mml_ = std::move(macros_.front().mml);
	macros_.clear();
	SetInitReadCount(mml_.size());
}

bool SourceFile::IsEmpty() const {
	return sv_.empty();
}

void SourceFile::Unput() {
	sv_ = prev_;
}

void SourceFile::AddMacro(const std::string &key, const std::string &repl) {
	repl_[key] = repl;
}

bool SourceFile::PushMacro(std::string_view key, std::string_view repl) {
	for (const auto &x : macros_)
		if (x.key == key)
			return false;
	std::size_t len = GetReadCount() + key.size();
	macros_.push_back(MacroState {key, std::move(mml_), len});
	prev_ = sv_ = mml_ = repl;
	return true;
}

bool SourceFile::PopMacro() {
	if (macros_.empty())
		return false;
	mml_ = std::move(macros_.back().mml);
	SetInitReadCount(macros_.back().charCount);
	macros_.pop_back();
	return true;
}

bool SourceFile::HasNextToken() {
	SkipSpaces();
	if (!DoReplacement())		// // //
		throw AMKd::Utility::SyntaxException {"Infinite replacement macro substitution."};
	SkipSpaces();
	return !IsEmpty();
}

bool SourceFile::DoReplacement() {
	while (true) {
		auto it = std::find_if(repl_.cbegin(), repl_.cend(), [&] (const auto &x) {
			const std::string &rhs = x.first;
			return std::string_view(sv_.data(), rhs.length()) == rhs;
		});
		if (it == repl_.cend())
			break;
		if (!PushMacro(it->first, it->second))
			return false;
	}
	return true;
}

std::size_t SourceFile::GetLineNumber() const {
	return std::count(mml_.cbegin(), mml_.cend(), '\n') -
		std::count(sv_.cbegin(), sv_.cend(), '\n') + 1;
}

std::size_t SourceFile::GetReadCount() const {
	return mml_.size() - sv_.size();
}

void SourceFile::SetReadCount(std::size_t count) {
	prev_ = sv_;
	sv_ = mml_;
	sv_.remove_prefix(count);
}

void SourceFile::SetInitReadCount(std::size_t count) {
	sv_ = mml_;
	sv_.remove_prefix(count);
	prev_ = sv_;
}

SourceFile::operator bool() const {
	return !IsEmpty();
}
