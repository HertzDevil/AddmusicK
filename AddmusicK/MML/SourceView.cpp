#include "SourceView.h"
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

	const std::string EMPTY;
}

using namespace AMKd::MML;

SourceView::SourceView() : mml_(EMPTY), sv_(mml_), prev_(sv_), repl_(replComp)
{
}

SourceView::SourceView(std::string_view data) :
	mml_(data), sv_(mml_), prev_(sv_), repl_(replComp)
{
	Trim("\xEF\xBB\xBF"); // utf-8 bom
}

SourceView::SourceView(const char *buf, std::size_t size) :
	SourceView(std::string_view(buf, size))
{
}

std::optional<std::string> SourceView::Trim(std::string_view re, bool ignoreCase) {
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

std::optional<std::string> SourceView::TrimUntil(std::string_view re, bool ignoreCase) {
	std::cmatch match;
	std::optional<std::string> z;

	prev_ = sv_;
	if (std::regex_search(sv_.data(), match, get_re(re, ignoreCase))) {
		z = match.prefix().str();
		sv_.remove_prefix(match.prefix().length() + match[0].length());
	}

	return z;
}

bool SourceView::Trim(char ch) {
	prev_ = sv_;
	if (!sv_.empty() && sv_.front() == ch) {
		sv_.remove_prefix(1);
		return true;
	}

	return false;
}

bool SourceView::SkipSpaces() {
	bool ret = false;
	do {
		ret = !Trim(R"(\s*)")->empty() || ret;
	} while (sv_.empty() && PopMacro());
	return ret;
}

void SourceView::Clear() {
	if (!macros_.empty())
		mml_ = macros_.front().mml;
	macros_.clear();
	SetInitReadCount(mml_.size());
}

bool SourceView::IsEmpty() const {
	return sv_.empty();
}

void SourceView::Unput() {
	sv_ = prev_;
}

void SourceView::AddMacro(const std::string &key, const std::string &repl) {
	repl_[key] = repl;
}

bool SourceView::PushMacro(std::string_view key, std::string_view repl) {
	for (const auto &x : macros_)
		if (x.key == key)
			return false;
	std::size_t len = GetReadCount() + key.size();
	macros_.push_back(MacroState {key, mml_, len});
	prev_ = sv_ = mml_ = repl;
	return true;
}

bool SourceView::PopMacro() {
	if (macros_.empty())
		return false;
	mml_ = macros_.back().mml;
	SetInitReadCount(macros_.back().charCount);
	macros_.pop_back();
	return true;
}

bool SourceView::HasNextToken() {
	SkipSpaces();
	if (!DoReplacement())		// // //
		throw AMKd::Utility::SyntaxException {"Infinite replacement macro substitution."};
	SkipSpaces();
	return !IsEmpty();
}

bool SourceView::DoReplacement() {
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

std::size_t SourceView::GetLineNumber() const {
	return std::count(mml_.cbegin(), mml_.cend(), '\n') -
		std::count(sv_.cbegin(), sv_.cend(), '\n') + 1;
}

std::size_t SourceView::GetReadCount() const {
	return mml_.size() - sv_.size();
}

void SourceView::SetReadCount(std::size_t count) {
	prev_ = sv_;
	sv_ = mml_;
	sv_.remove_prefix(count);
}

void SourceView::SetInitReadCount(std::size_t count) {
	sv_ = mml_;
	sv_.remove_prefix(count);
	prev_ = sv_;
}

SourceView::operator bool() const {
	return !IsEmpty();
}
