#pragma once

#include "Core.h"

namespace AMKd::MML::Lexer {

template <typename T>
struct Option
{
	using arg_type = std::optional<typename T::arg_type>;
	std::optional<arg_type> operator()(SourceView &file) {
		return std::optional<arg_type>(arg_type {T()(file)});
	}
};

} // namespace AMKd::MML::Lexer
