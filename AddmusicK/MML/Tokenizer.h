#pragma once

#include "SourceView.h"
#include "../Utility/Trie.h"
#include <optional>

namespace AMKd::MML::Lexer {

struct Tokenizer
{
	template <typename T>
	std::optional<T> operator()(SourceView &file, const AMKd::Utility::Trie<T> &cmds) const {
		std::optional<T> result;
		file.TryProcess([&] (std::string_view &sv) {
			if (std::optional<T> tok = cmds.SearchValue(sv)) {
				result = std::move(tok);
				return true;
			}
			return false;
		});
		return result;
	}
};

} // namespace AMKd::MML::Lexer
