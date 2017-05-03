#include "Lexer.h"
#include <regex>

using namespace AMKd::MML;

LEXER_FUNC_START(Int)
//	if (file.Trim("\\$"))
//		return Hex2()(file);
	if (auto x = file.Trim("[[:digit:]]+")) {
		auto ret = static_cast<arg_type>(std::strtoul(x->c_str(), nullptr, 10));
		if (errno != ERANGE)
			return {ret, true};
	}
LEXER_FUNC_END()

LEXER_FUNC_START(SInt)
	if (auto x = file.Trim("-?[[:digit:]]+")) {
		auto ret = static_cast<arg_type>(std::strtol(x->c_str(), nullptr, 10));
		if (errno != ERANGE)
			return {ret, true};
	}
LEXER_FUNC_END()

LEXER_FUNC_START(Byte)
	if (auto x = file.Trim("\\$[[:xdigit:]]{2}")) {
		auto ret = static_cast<arg_type>(std::strtoul(x->c_str() + 1, nullptr, 16));
		if (errno != ERANGE)
			return {ret, true};
	}
LEXER_FUNC_END()

LEXER_FUNC_START(Ident)
	if (auto x = file.Trim("[_[:alpha:]]\\w*"))
		return {*x, true};
LEXER_FUNC_END()

LEXER_FUNC_START(QString)
	const std::regex ESC1 {R"(\\\\)"};
	const std::regex ESC2 {R"(\\")"};
	if (auto x = file.Trim(R"/("([^\\"]|\\\\|\\")*")/"))
		return {std::regex_replace(std::regex_replace(*x, ESC1, "\\"), ESC2, "\""), true};
LEXER_FUNC_END()

LEXER_FUNC_START(Time)
	if (file.Trim("auto"))
		return {0, true};
	if (auto x = file.Trim("[[:digit:]]{1,2}"))
		if (file.Trim(':'))
			if (auto y = file.Trim("[[:digit:]]{2}")) {
				auto m = static_cast<arg_type>(std::strtoul(x->c_str(), nullptr, 10));
				auto s = static_cast<arg_type>(std::strtoul(y->c_str(), nullptr, 10));
				return {m * 60 + s, true};
			}
LEXER_FUNC_END()

#undef LEXER_DECL
#undef LEXER_FUNC_START
#undef LEXER_FUNC_END
