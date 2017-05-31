#pragma once

#include "../Lexer.h"
#include <variant>
#include <type_traits>

namespace AMKd::MML::Lexer {

namespace details {

template <typename...>
struct select_tag_t;

template <typename, typename, typename...>
struct SelectImpl;

template <std::size_t I, std::size_t... Js, typename T, typename... Us, typename... Vs>
struct SelectImpl<std::index_sequence<I, Js...>, select_tag_t<Vs...>, T, Us...>
{
	using arg_type = std::variant<typename Vs::arg_type...>;
	std::optional<arg_type> operator()(SourceFile &file) {
		if (auto param = T()(file))
			return arg_type {std::in_place_index_t<I>(), std::move(*param)};
		return SelectImpl<std::index_sequence<Js...>, select_tag_t<Vs...>, Us...>()(file);
	}
};

template <typename... Args>
struct SelectImpl<std::index_sequence<>, select_tag_t<Args...>>
{
	using arg_type = std::variant<typename Args::arg_type...>;
	std::optional<arg_type> operator()(SourceFile &) {
		return std::nullopt;
	}
};

} // namespace details

template <typename... Args>
struct Select
{
private:
	using impl_t = details::SelectImpl<std::index_sequence_for<Args...>,
		details::select_tag_t<Args...>, Args...>;
public:
	using arg_type = typename impl_t::arg_type;
	std::optional<arg_type> operator()(SourceFile &file) {
		return impl_t()(file);
	}
};

} // namespace AMKd::MML::Lexer
