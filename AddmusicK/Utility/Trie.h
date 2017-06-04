#pragma once

#include <optional>
#include <string_view>
#include <unordered_map>
#include <memory>

namespace AMKd::Utility {

template <typename V, typename K = char>
class Trie
{
	using key_type = K;
	using value_type = V;
	enum { npos = std::basic_string_view<K>::npos };

private:
	struct Node
	{
		std::optional<V> val_;
		std::unordered_map<K, std::unique_ptr<Node>> child_;
	};

public:
	Trie() = default;

	void Insert(std::basic_string_view<K> str, const V &data) {
		GetNode(str)->val_ = data;
	}

	std::optional<V> GetValue(std::basic_string_view<K> str) const {
		if (const Node *n = GetNode(str))
			return n->val_;
		return { };
	}

	int SearchIndex(std::basic_string_view<K> str) const {
		int z = npos;
		int len = 0;
		Traverse(str, [&] (const Node &n) {
			if (n.val_.has_value())
				z = len;
			++len;
		});
		return z;
	}

	std::optional<V> SearchValue(std::basic_string_view<K> &str) const {
		std::optional<V> z;
		std::basic_string_view<K> best = str;
		Traverse(best, [&] (const Node &n) {
			if (n.val_.has_value()) {
				z = n.val_;
				str = best;
			}
		});
		return z;
	}

private:
	template <typename F>
	void Traverse(std::basic_string_view<K> &str, F &&f) const {
		const Node *current = &head_;
		while (true) {
			f(*current);
			if (str.empty())
				return;
			K ch = str.front();
			auto it = current->child_.find(ch);
			if (it == current->child_.end())
				return;
			current = it->second.get();
			str.remove_prefix(1);
		}
	}

	Node *GetNode(std::basic_string_view<K> str) {
		Node *current = &head_;
		while (!str.empty()) {
			K ch = str.front();
			auto it = current->child_.find(ch);
			if (it == current->child_.end())
				current->child_[ch] = std::make_unique<Node>();
			current = current->child_[ch].get();
			str.remove_prefix(1);
		}
		return current;
	}

	const Node *GetNode(std::basic_string_view<K> str) const {
		const Node *current = nullptr;
		Traverse(str, [&] (const Node &n) { current = &n; });
		return current;
	}

	Node head_;
};

} // namespace AMKd::Utility
