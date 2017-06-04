#pragma once

#include "Core.h"
#include <vector>

namespace AMKd::MML::Lexer {

template <typename... Args>
struct Multi
{
private:
	using info = details::lexer_info<0, void, Args...>;
public:
	using arg_type = std::vector<typename info::result_type::tuple_type>;
	std::optional<arg_type> operator()(SourceFile &file) {
		arg_type ret;
		while (auto &&param = GetParameters<Args...>(file))
			ret.push_back(*param);
		return ret;
	}
};

} // namespace AMKd::MML::Lexer
