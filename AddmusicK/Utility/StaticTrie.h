#pragma once

#include <string_view>
#include <utility>

namespace AMKd::Utility {

template <typename, char...>
struct TrieEntry { };

namespace details {

using trie_result_t = bool;

template <char, typename>
struct trie_pair { };

template <typename...>
struct trie_node;
template <typename, typename...>
struct trie_val_node;

#ifdef _MSVC_LANG
template <char K, typename V>
struct trie_node<trie_pair<K, V>>
{
	template <typename... Args>
	static trie_result_t
	search(std::string_view &str, size_t offs, Args&&... args) {
		return str[offs] == K ?
			V::search(str, offs + 1, std::forward<Args>(args)...) :
			trie_result_t { };
	}
};
template <typename T, char K, typename V>
struct trie_val_node<T, trie_pair<K, V>>
{
	template <typename... Args>
	static trie_result_t
	search(std::string_view &str, size_t offs, Args&&... args) {
		return str[offs] == K ?
			V::search(str, offs + 1, std::forward<Args>(args)...) :
			trie_val_node<T>::search(str, offs, std::forward<Args>(args)...);
	}
};
#endif
template <char K, typename V, char... Ks, typename... Vs>
struct trie_node<trie_pair<K, V>, trie_pair<Ks, Vs>...>
{
	template <typename... Args>
	static trie_result_t
	search(std::string_view &str, size_t offs, Args&&... args) {
		using next_t = trie_node<trie_pair<Ks, Vs>...>;
		return str[offs] == K ?
			V::search(str, offs + 1, std::forward<Args>(args)...) :
			next_t::search(str, offs, std::forward<Args>(args)...);
	}
};
template <typename T, char K, typename V, char... Ks, typename... Vs>
struct trie_val_node<T, trie_pair<K, V>, trie_pair<Ks, Vs>...>
{
	template <typename... Args>
	static trie_result_t
	search(std::string_view &str, size_t offs, Args&&... args) {
		using next_t = trie_val_node<T, trie_pair<Ks, Vs>...>;
		return str[offs] == K ?
			V::search(str, offs + 1, std::forward<Args>(args)...) :
			next_t::search(str, offs, std::forward<Args>(args)...);
	}
};

template <>
struct trie_node<>
{
	template <typename... Args>
	static trie_result_t search(std::string_view &, size_t, Args&&...) {
		return trie_result_t { };
	}
};
template <typename T>
struct trie_val_node<T>
{
	template <typename... Args>
	static trie_result_t
	search(std::string_view &str, size_t offs, Args&&... args) {
		T()(std::as_const(str), offs, std::forward<Args>(args)...);
		str.remove_prefix(offs);
		return true;
	}
};

template <typename, typename>
struct concat_node;
template <typename... Ts, typename... Us>
struct concat_node<trie_node<Ts...>, trie_node<Us...>>
{
	using type = trie_node<Ts..., Us...>;
};
template <typename... Ts, typename... Us, typename V>
struct concat_node<trie_val_node<V, Ts...>, trie_val_node<V, Us...>>
{
	using type = trie_val_node<V, Ts..., Us...>;
};

template <typename, typename, char...>
struct join_node;
template <typename T, typename U, char... Cs>
using join_node_t = typename join_node<T, U, Cs...>::type;
template <typename... Args, typename T>
struct join_node<trie_node<Args...>, T>
{
	using type = trie_val_node<T, Args...>;
};
template <typename... Args, typename T, typename U>
struct join_node<trie_val_node<U, Args...>, T>
{
};
template <typename T, char C, char... Cs>
struct join_node<trie_node<>, T, C, Cs...>
{
	using type = trie_node<trie_pair<C, join_node_t<trie_node<>, T, Cs...>>>;
};
template <typename T, typename U, char C, char... Cs>
struct join_node<trie_val_node<U>, T, C, Cs...>
{
	using type = trie_val_node<U, trie_pair<C,
		join_node_t<trie_node<>, T, Cs...>>>;
};
template <typename T, char C, char... Cs, typename AT, typename... ATs>
struct join_node<trie_node<AT, ATs...>, T, C, Cs...>
{
	using type = typename concat_node<trie_node<AT>,
		join_node_t<trie_node<ATs...>, T, C, Cs...>>::type;
};
template <typename T, typename U, char C, char... Cs,
	typename AT, typename... ATs>
	struct join_node<trie_val_node<U, AT, ATs...>, T, C, Cs...>
{
	using type = typename concat_node<trie_val_node<U, AT>,
		join_node_t<trie_val_node<U, ATs...>, T, C, Cs...>>::type;
};
template <typename T, char C, char... Cs, typename V, typename... ATs>
struct join_node<trie_node<trie_pair<C, V>, ATs...>, T, C, Cs...>
{
	using type = trie_node<trie_pair<C, join_node_t<V, T, Cs...>>, ATs...>;
};
template <typename T, typename U, char C, char... Cs,
	typename V, typename... ATs>
	struct join_node<trie_val_node<U, trie_pair<C, V>, ATs...>, T, C, Cs...>
{
	using type = trie_val_node<
		U, trie_pair<C, join_node_t<V, T, Cs...>>, ATs...>;
};

template <typename...>
struct make_trie;
template <>
struct make_trie<>
{
	using type = details::trie_node<>;
};
template <typename T, char... Cs, typename... Args>
struct make_trie<TrieEntry<T, Cs...>, Args...>
{
	using type = details::join_node_t<
		typename make_trie<Args...>::type, T, Cs...>;
};

template <typename T>
struct SourceViewAdaptor
{
	template <typename... Args>
	void operator()(const std::string_view &, std::size_t, Args&&... args) {
		return T()(std::forward<Args>(args)...);
	}
};

} // namespace details

template <typename... Ts>
using StaticTrie = typename details::make_trie<Ts...>::type;

template <typename Tr, typename... Args>
details::trie_result_t ParseTrie(Tr, std::string_view &str, Args&&... args) {
	return Tr::search(str, 0, std::forward<Args>(args)...);
}

template <typename T, char... Cs>
using EntryAdaptor = TrieEntry<details::SourceViewAdaptor<T>, Cs...>;

} // namespace AMKd::Utility
