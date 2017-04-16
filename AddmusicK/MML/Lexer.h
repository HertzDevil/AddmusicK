#pragma once

#include <tuple>
#include <utility>
#include <initializer_list>
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
	file.Trim("\\s*");
//	if (I && !file.Trim(",?\\s*"))
	if (I && !file.Trim(",\\s*"))
		return false;
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
std::tuple<typename Arg::arg_type...>
GetParameters(SourceFile &file) {
	std::tuple<typename Arg::arg_type...> out;

	if (details::get_impl<decltype(out), Arg...>(file, out, std::index_sequence_for<Arg...> { }))
		return out;

	// file.PrintError(...);
	return std::tuple<typename Arg::arg_type...> { };
}

#define LEXER_DECL(T, U) \
	struct T \
	{ \
		using arg_type = U; \
		std::pair<arg_type, bool> operator()(SourceFile &file); \
	};

LEXER_DECL(Int, unsigned)
LEXER_DECL(SInt, int)
LEXER_DECL(Hex2, int)

#undef LEXER_DECL

} // namespace Lexer

} // namespace AMKd::MML
