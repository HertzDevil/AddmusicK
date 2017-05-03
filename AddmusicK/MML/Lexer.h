#pragma once

#include <tuple>
#include <utility>
#include <initializer_list>
#include <cstdlib>
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
	std::optional<typename T::arg_type> ret = T()(file);
	if (ret.has_value())
		std::get<I>(tup) = *ret;
	return ret.has_value();
}

template <typename T, typename... L, std::size_t... I>
bool get_impl(SourceFile &file, T &tup, std::index_sequence<I...>) {
	return conjunction({get_step<L, T, I>(file, tup)...});
}

} // namespace details

// skips spaces before each token
template <typename... Arg>
std::optional<std::tuple<typename Arg::arg_type...>>
GetParameters(SourceFile &file) {
	std::tuple<typename Arg::arg_type...> out;
	std::size_t p = file.GetReadCount();

	if (details::get_impl<decltype(out), Arg...>(file, out, std::index_sequence_for<Arg...> { }))
		return out;

	// file.PrintError(...);
	file.SetReadCount(p);
	return std::nullopt;
}

#define LEXER_DECL(T, U) \
	struct T \
	{ \
		using arg_type = U; \
		std::optional<arg_type> operator()(SourceFile &file); \
	};

#define LEXER_FUNC_START(T) \
	typename std::optional<typename T::arg_type> T::operator()(SourceFile &file) {

#define LEXER_FUNC_END(...) \
		return std::nullopt; \
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
	std::optional<arg_type> operator()(SourceFile &file) {
		if (auto v = T()(file))
			return *v;
		return std::nullopt;
	}
};

template <char C>
struct Sep
{
	using arg_type = char;
	std::optional<arg_type> operator()(SourceFile &file) {
		if (file.Trim(C))
			return C;
		return std::nullopt;
	}
};
using Comma = Sep<','>;

template <size_t N>
struct Hex
{
	static_assert(N > 0u && N <= 8u, "Invalid hex lexer size");
	static constexpr char fmt[] = {
		'[', '[', ':', 'x', 'd', 'i', 'g', 'i', 't', ':', ']', ']', '{', '0' + N, '}', '\0',
	}; // "[[:xdigit:]]{N}"
	using arg_type = unsigned;
	std::optional<arg_type> operator()(SourceFile &file) {
		if (auto x = file.Trim(fmt))
			return static_cast<arg_type>(std::strtol(x->c_str(), nullptr, 16));
		return std::nullopt;
	}
};

} // namespace Lexer

} // namespace AMKd::MML
