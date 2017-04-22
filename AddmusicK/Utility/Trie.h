#pragma once

#include <optional>
#include <string_view>
#include <map>
#include <memory>

namespace AMKd::Utility {

template <typename V, typename K = char>
class Trie
{
	using key_type = K;
	using value_type = V;
	enum { npos = std::basic_string_view<key_type>::npos };

private:
	struct Node
	{
		std::optional<value_type> val_;
		std::map<key_type, Node> child_;
	};

public:
	Trie() = default;

	void Insert(std::basic_string_view<key_type> str, const value_type &data) {
		GetNode(str)->val_ = data;
	}

	std::optional<value_type> GetValue(std::basic_string_view<key_type> str) const {
		if (const Node *n = GetNode(str))
			return n->val_;
		return { };
	}

	int SearchIndex(std::basic_string_view<key_type> str) const {
		int z = npos;
		int len = 0;
		Traverse(str, [&] (const Node &n) {
			if (n.val_.has_value())
				z = len;
			++len;
		});
		return z;
	}

	std::optional<value_type> SearchValue(std::basic_string_view<key_type> &str) const {
		std::optional<value_type> z;
		std::basic_string_view<key_type> best = str;
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
	void Traverse(std::basic_string_view<key_type> &str, F &&f) const {
		const Node *current = &head_;
		while (true) {
			f(*current);
			if (str.empty())
				return;
			key_type ch = str.front();
			auto it = current->child_.find(ch);
			if (it == current->child_.end())
				return;
			current = &it->second;
			str.remove_prefix(1);
		}
	}

	Node *GetNode(std::basic_string_view<key_type> str) {
		Node *current = &head_;
		while (!str.empty()) {
			key_type ch = str.front();
			auto it = current->child_.find(ch);
			if (it == current->child_.end())
				current->child_[ch] = Node { };
			current = &current->child_[ch];
			str.remove_prefix(1);
		}
		return current;
	}

	const Node *GetNode(std::basic_string_view<key_type> str) const {
		const Node *current = nullptr;
		Traverse(str, [&] (const Node &n) { current = &n; });
		return n;
	}

	Node head_;
};

} // namespace AMKd::Utility
