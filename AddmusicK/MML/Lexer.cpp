#include "Lexer.h"
#include <cerrno>
#include <regex>

using namespace AMKd::MML;

LEXER_FUNC_START(Lexer::Int)
//	if (file.Trim("\\$"))
//		return Hex2()(file);
	if (auto x = file.Trim("[[:digit:]]+")) {
		auto ret = static_cast<arg_type>(std::strtoul(x->c_str(), nullptr, 10));
		if (errno != ERANGE)
			return ret;
	}
LEXER_FUNC_END()

LEXER_FUNC_START(Lexer::HexInt)
	if (auto x = file.Trim("[[:xdigit:]]+")) {
		auto ret = static_cast<arg_type>(std::strtoul(x->c_str(), nullptr, 16));
		if (errno != ERANGE)
			return ret;
	}
LEXER_FUNC_END()

LEXER_FUNC_START(Lexer::SInt)
	if (auto x = file.Trim("[+-]?[[:digit:]]+")) {
		auto ret = static_cast<arg_type>(std::strtol(x->c_str(), nullptr, 10));
		if (errno != ERANGE)
			return ret;
	}
LEXER_FUNC_END()

LEXER_FUNC_START(Lexer::Byte)
	if (auto x = file.Trim("\\$[[:xdigit:]]{2}")) {
		auto ret = static_cast<arg_type>(std::strtoul(x->c_str() + 1, nullptr, 16));
		if (errno != ERANGE)
			return ret;
	}
LEXER_FUNC_END()

LEXER_FUNC_START(Lexer::Ident)
	if (auto x = file.Trim("[_[:alpha:]]\\w*"))
		return *x;
LEXER_FUNC_END()

LEXER_FUNC_START(Lexer::String)
	if (auto x = file.Trim("\\S+"))
		return *x;
LEXER_FUNC_END()

LEXER_FUNC_START(Lexer::QString)
	const std::regex ESC1 {R"(\\\\)"};
	const std::regex ESC2 {R"(\\")"};
//	if (auto x = file.Trim(R"/("([^\\"]|\\\\|\\")*")/"))
	if (auto x = file.Trim(R"/(([^\\"]|\\\\|\\")*")/")) {
		auto str = std::regex_replace(std::regex_replace(*x, ESC1, "\\"), ESC2, "\"");
		str.pop_back(); // final quotation mark
		return str;
	}
LEXER_FUNC_END()

LEXER_FUNC_START(Lexer::Time)
	if (file.Trim("auto"))
		return 0;
	if (auto x = file.Trim("[[:digit:]]{1,2}"))
		if (file.Trim(':'))
			if (auto y = file.Trim("[[:digit:]]{2}")) {
				auto m = static_cast<arg_type>(std::strtoul(x->c_str(), nullptr, 10));
				auto s = static_cast<arg_type>(std::strtoul(y->c_str(), nullptr, 10));
				return m * 60 + s;
			}
LEXER_FUNC_END()

LEXER_FUNC_START(Lexer::Dur)
	double mult = 0.;
	double add = 0.;
	double lastMult = 0.;
	double lastAdd = 0.;
	using namespace Lexer;

	do {
		if (GetParameters<Sep<'='>>(file))
			if (auto param = GetParameters<Int>(file)) {
				lastMult = 0.;
				add += (lastAdd = param.get<0>());
			}
			else
				return std::nullopt;
		else {
			auto param = GetParameters<Int>(file);
			int dots = 0;
			while (GetParameters<Sep<'.'>>(file))
				++dots;
			double factor = 2. - std::pow(.5, dots);

			if (param)
				if (auto len = param.get<0>()) {
					lastMult = 0.;
					add += (lastAdd = Duration::WHOLE_NOTE_TICKS * factor / len);
				}
				else
					return std::nullopt;
			else {
				mult += (lastMult = factor);
				lastAdd = 0.;
			}
		}
	} while (GetParameters<Sep<'^'>>(file));

	return Duration {mult, add, lastMult, lastAdd};
LEXER_FUNC_END()

LEXER_FUNC_START(Lexer::RestDur)
	double mult = 0.;
	double add = 0.;
	double lastMult = 0.;
	double lastAdd = 0.;
	using namespace Lexer;

	do {
		if (GetParameters<Sep<'='>>(file))
			if (auto param = GetParameters<Int>(file)) {
				lastMult = 0.;
				add += (lastAdd = param.get<0>());
			}
			else
				return std::nullopt;
		else {
			auto param = GetParameters<Int>(file);
			int dots = 0;
			while (GetParameters<Sep<'.'>>(file))
				++dots;
			double factor = 2. - std::pow(.5, dots);

			if (param)
				if (auto len = param.get<0>()) {
					lastMult = 0.;
					add += (lastAdd = Duration::WHOLE_NOTE_TICKS * factor / len);
				}
				else
					return std::nullopt;
			else {
				mult += (lastMult = factor);
				lastAdd = 0.;
			}
		}
	} while (GetParameters<Sep<'^'>>(file) || GetParameters<Sep<'r'>>(file)); // merge rests here

	return Duration {mult, add, lastMult, lastAdd};
LEXER_FUNC_END()

#undef LEXER_DECL
#undef LEXER_FUNC_START
#undef LEXER_FUNC_END
