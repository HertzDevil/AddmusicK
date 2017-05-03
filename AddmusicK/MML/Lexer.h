#pragma once

#include <tuple>
#include <utility>
#include <initializer_list>
#include <cstdlib>
#include <cerrno>
#include "SourceFile.h"

namespace AMKd::MML {

class SourceFile;

namespace Lexer {

namespace details {

inline constexpr bool conjunction(std::initializer_list<bool> bl) {
	for (bool b : bl)
		if (!b)
			return false;
	return true;
}

template <typename T, typename U, std::size_t I>
bool get_step(SourceFile &file, U &tup) {
	file.SkipSpaces();
	std::pair<typename T::arg_type, bool> ret = T()(file);
	if (!ret.second)
		return false;
	std::get<I>(tup) = ret.first;
	return true;
}

template <typename T, typename... L, std::size_t... I>
bool get_impl(SourceFile &file, T &tup, std::index_sequence<I...>) {
	return conjunction({get_step<L, T, I>(file, tup)...});
}

} // namespace details

template <typename... Arg>
std::optional<std::tuple<typename Arg::arg_type...>>
GetParameters(SourceFile &file) {
	std::tuple<typename Arg::arg_type...> out;
	std::size_t p = file.GetReadCount();

	if (details::get_impl<decltype(out), Arg...>(file, out, std::index_sequence_for<Arg...> { }))
		return out;

	// file.PrintError(...);
	file.SetReadCount(p);
	return { };
}

#define LEXER_DECL(T, U) \
	struct T \
	{ \
		using arg_type = U; \
		std::pair<arg_type, bool> operator()(SourceFile &file); \
	};

#define LEXER_FUNC_START(T) \
	std::pair<Lexer::T::arg_type, bool> \
	Lexer::T::operator()(SourceFile &file) {

#define LEXER_FUNC_END(...) \
		return {arg_type(), false}; \
	}

LEXER_DECL(Int, unsigned)
LEXER_DECL(SInt, int)
LEXER_DECL(Byte, int)
LEXER_DECL(Ident, std::string)
LEXER_DECL(QString, std::string)
LEXER_DECL(Time, int)

template <typename T>
struct Opt
{
	using arg_type = std::optional<typename T::arg_type>;
	std::pair<arg_type, bool> operator()(SourceFile &file) {
		auto v = T()(file);
		if (v.second)
			return {v.first, true};
		return {std::nullopt, true};
	}
};

template <size_t N>
struct Hex
{
	static_assert(N <= 8, "Invalid hex lexer size");
	using arg_type = unsigned;
	std::pair<arg_type, bool> operator()(SourceFile &file) {
		constexpr const char fmt[] = {
			'[', '[', ':', 'x', 'd', 'i', 'g', 'i', 't', ':', ']', ']', '{', '0' + N, '}', '\0',
		};
		if (auto x = file.Trim(fmt))
			return {static_cast<arg_type>(std::strtol(x->c_str(), nullptr, 16)), true};
		return {'\0', false};
	}
};

template <char C>
struct Sep
{
	using arg_type = char;
	std::pair<arg_type, bool> operator()(SourceFile &file) {
		if (file.Trim(C))
			return {C, true};
		return {'\0', false};
	}
};

using Comma = Sep<','>;

} // namespace Lexer

} // namespace AMKd::MML
