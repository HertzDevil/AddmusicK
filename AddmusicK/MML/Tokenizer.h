#pragma once

#include "SourceFile.h"
#include "../Utility/Trie.h"
#include <optional>

namespace AMKd::MML {

struct Tokenizer
{
	template <typename T>
	std::optional<T> operator()(SourceFile &file, const AMKd::Utility::Trie<T> &cmds) const {
		file.prev_ = file.sv_;
		if (std::optional<T> result = cmds.SearchValue(file.sv_))
			return result;
		file.sv_ = file.prev_;
		return std::nullopt;
	}
};

} // AMKd::MML
