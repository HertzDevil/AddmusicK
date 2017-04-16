#include "Lexer.h"
#include <cstdlib>
#include <cerrno>

using namespace AMKd::MML;

#define LEXER_FUNC_START(T) \
	std::pair<Lexer::T::arg_type, bool> \
	Lexer::T::operator()(SourceFile &file) {

#define LEXER_FUNC_END \
		return {arg_type(), false}; \
	}

LEXER_FUNC_START(Int)
	if (file.Trim("\\$"))
		return Hex2()(file);
	if (auto x = file.Trim("[0-9]+")) {
		auto ret = static_cast<arg_type>(std::strtoul(x->c_str(), nullptr, 10));
		if (errno != ERANGE)
			return {ret, true};
	}
LEXER_FUNC_END

LEXER_FUNC_START(SInt)
	if (auto x = file.Trim("-?[0-9]+")) {
		auto ret = static_cast<arg_type>(std::strtol(x->c_str(), nullptr, 10));
		if (errno != ERANGE)
			return {ret, true};
	}
LEXER_FUNC_END

LEXER_FUNC_START(Hex2)
	if (auto x = file.Trim("[0-9A-Fa-f][0-9A-Fa-f]")) {
		auto ret = static_cast<arg_type>(std::strtoul(x->c_str(), nullptr, 16));
		if (errno != ERANGE)
			return {ret, true};
	}
LEXER_FUNC_END

#undef LEXER_FUNC_START
#undef LEXER_FUNC_END
