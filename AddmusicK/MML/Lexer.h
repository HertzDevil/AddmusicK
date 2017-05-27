#pragma once

#include <tuple>
#include <utility>
#include <initializer_list>
#include <cstdlib>
#include "SourceFile.h"
#include "Duration.h"

namespace AMKd::MML {

class SourceFile;

namespace Lexer {

template <typename>
struct Bypass : std::false_type { };

template <typename... Args>
struct LexerResult
{
	using tuple_type = std::tuple<Args...>;
	std::optional<tuple_type> result;
	constexpr const tuple_type &operator*() const & {
		return *result;
	}
	constexpr tuple_type &&operator*() && {
		return std::move(*result);
	}
	template <std::size_t N>
	constexpr const auto &get() const &noexcept {
		return std::get<N>(*result);
	}
	template <std::size_t N>
	constexpr auto &&get() &&noexcept {
		return std::move(std::get<N>(*result));
	}
	constexpr bool success() const noexcept {
		return result.has_value();
	}
	constexpr explicit operator bool() const noexcept {
		return success();
	}
};

namespace details {

template <std::size_t, typename>
struct prepend;
template <std::size_t X, std::size_t... Ys>
struct prepend<X, std::index_sequence<Ys...>>
{
	using type = std::index_sequence<X, Ys...>;
};

template <typename...>
struct res_prepend;
template <typename T, typename... Us>
struct res_prepend<T, LexerResult<Us...>>
{
	using type = LexerResult<T, Us...>;
};

template <std::size_t, typename, typename...>
struct lexer_info;
template <std::size_t X>
struct lexer_info<X, void>
{
	static constexpr std::size_t count = 0;
	using index_type = std::index_sequence<>;
	using result_type = LexerResult<>;
};
template <std::size_t X, typename T, typename... Us>
struct lexer_info<X, std::enable_if_t<!Bypass<T>::value>, T, Us...>
{
private:
	using next = lexer_info<X + 1, void, Us...>;
public:
	static constexpr std::size_t count = 1 + next::count;
	using index_type = typename prepend<X, typename next::index_type>::type;
	using result_type = typename res_prepend<
		typename T::arg_type, typename next::result_type>::type;
};
template <std::size_t X, typename T, typename... Us>
struct lexer_info<X, std::enable_if_t<Bypass<T>::value>, T, Us...>
{
private:
	using next = lexer_info<X, void, Us...>;
public:
	static constexpr std::size_t count = next::count;
	using index_type = typename prepend<-1, typename next::index_type>::type;
	using result_type = typename next::result_type;
};

template <typename T, typename U, std::size_t I>
struct tup_assigner
{
	bool operator()(SourceFile &file, U &tup) const {
		file.SkipSpaces();
		std::optional<typename T::arg_type> ret = T()(file);
		if (ret.has_value())
			std::get<I>(tup) = *ret;
		return ret.has_value();
	}
};
template <typename T, typename U>
struct tup_assigner<T, U, -1>
{
	bool operator()(SourceFile &file, U &) const {
		file.SkipSpaces();
		return T()(file).has_value();
	}
};

#if 0
template <typename T, typename... L, std::size_t... I>
bool get_impl(SourceFile &file, T &tup, std::index_sequence<I...>) {
	return (... && tup_assigner<L, T, I>()(file, tup));
}
#else
template <typename T, typename... L>
bool get_impl(SourceFile &, T &, std::index_sequence<>) {
	return true;
}

template <typename T, typename L, typename... Ls, std::size_t I, std::size_t... Is>
bool get_impl(SourceFile &file, T &tup, std::index_sequence<I, Is...>) {
	return tup_assigner<L, T, I>()(file, tup) &&
		get_impl<T, Ls...>(file, tup, std::index_sequence<Is...>());
}
#endif

} // namespace details

// skips spaces before each token
template <typename... Arg>
typename details::lexer_info<0, void, Arg...>::result_type
GetParameters(SourceFile &file) {
	using info = details::lexer_info<0, void, Arg...>;
	typename info::result_type::tuple_type out;
	typename info::index_type indices;
	std::size_t p = file.GetReadCount();

	if (details::get_impl<decltype(out), Arg...>(file, out, indices))
		return typename info::result_type {out};

	file.SetReadCount(p);
	return typename info::result_type {std::nullopt};
}

#define BYPASS_DECL(T) \
	template <> \
	struct Bypass<T> : std::true_type { }

#define LEXER_DECL(T, U) \
	struct T \
	{ \
		using arg_type = U; \
		std::optional<arg_type> operator()(AMKd::MML::SourceFile &file); \
	};

#define LEXER_FUNC_START(T) \
	typename std::optional<typename T::arg_type> T::operator()(AMKd::MML::SourceFile &file) {

#define LEXER_FUNC_END() \
		return std::nullopt; \
	}

LEXER_DECL(Int, unsigned)
LEXER_DECL(HexInt, unsigned)
LEXER_DECL(SInt, int)
LEXER_DECL(Byte, unsigned)
LEXER_DECL(Ident, std::string)
LEXER_DECL(String, std::string)
LEXER_DECL(QString, std::string)
LEXER_DECL(Time, unsigned)
LEXER_DECL(Dur, Duration)
LEXER_DECL(RestDur, Duration) // variant supporting 'r', will be removed when Music::Actions can merge

template <typename T>
struct Opt
{
	using arg_type = std::optional<typename T::arg_type>;
	std::optional<arg_type> operator()(SourceFile &file) {
		return std::optional<arg_type>(arg_type {T()(file)});
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
template <char C>
struct Bypass<Sep<C>> : std::true_type { };
using Comma = Sep<','>;
BYPASS_DECL(Comma);

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

template <typename... Args> // for c++17 structured binding support
struct std::tuple_size<AMKd::MML::Lexer::LexerResult<Args...>> :
	std::integral_constant<std::size_t, sizeof...(Args)> { };
