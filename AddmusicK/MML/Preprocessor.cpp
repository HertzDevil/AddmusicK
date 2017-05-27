#include "Preprocessor.h"
#include "../globals.h"
#include "../Music.h" // CHANNELS
#include "Lexer.h"
#include "../Utility/Exception.h"

using namespace AMKd::MML;

#define MISSING(name) "Missing argument for " name " preprocessor directive."

namespace {
	enum class compare_t { EQ, NE, LT, LE, GT, GE };

	LEXER_DECL(Comp, compare_t)
	LEXER_FUNC_START(Comp)
		if (file.Trim('=') && file.Trim('='))
			return compare_t::EQ;
		if (file.Trim('<'))
			return file.Trim('=') ? compare_t::LE : compare_t::LT;
		if (file.Trim('>'))
			return file.Trim('=') ? compare_t::GE : compare_t::GT;
		if (file.Trim('!') && file.Trim('='))
			return compare_t::NE;
	LEXER_FUNC_END()
} // namespace

Preprocessor::Preprocessor(const std::string &str, const std::string &filename) :
	target(Target::Unknown), version(0), firstChannel(CHANNELS)
{
	// Handles #ifdefs.  Maybe more later?
	SourceFile text {str};

	okayStatus.push(true);

	try {
		while (text) {
			// don't use GetParameters because all whitespaces are to be kept verbatim
			std::string row_ = *text.Trim("[^\\r\\n]*");
			SourceFile row {row_};
			text.Trim('\r');

			std::string add = *row.Trim("\\s*");
			bool allowDirectives = true;

			const auto doDirective = [&] (auto &&f, auto&&... args) {
				if (!allowDirectives)
					throw AMKd::Utility::SyntaxException {"A preprocessor directive is not allowed here."};
				(this->*f)(std::forward<decltype(args)>(args)...);
				if (row.SkipSpaces(), row.Trim(';')) {
					bool setTitle = row.Trim("\\s*title\\s*=\\s*") && row;
					std::string rest = *row.Trim(".*");
					if (setTitle)
						title = rest;
				}
				else if (row)
					throw AMKd::Utility::SyntaxException {"Trailing text after preprocessor directive."};
			};

			while (row) {
				if (row.Trim('#')) {
					using namespace Lexer; // fine here
					auto spaces = *row.Trim("\\s*");
					if (auto ident = Ident()(row))
						if (*ident == "define") {
							auto param = GetParameters<Ident, Opt<SInt>>(row);
							param ? doDirective(&Preprocessor::doDefine, param.get<0>(), param.get<1>() ? *param.get<1>() : 1) :
								throw AMKd::Utility::SyntaxException {MISSING("#define")};
						}
						else if (*ident == "undef") {
							auto param = GetParameters<Ident>(row);
							param ? doDirective(&Preprocessor::doUndef, param.get<0>()) :
								throw AMKd::Utility::SyntaxException {MISSING("#undef")};
						}
						else if (*ident == "ifdef") {
							auto param = GetParameters<Ident>(row);
							param ? doDirective(&Preprocessor::doIfdef, param.get<0>()) :
								throw AMKd::Utility::SyntaxException {MISSING("#ifdef")};
						}
						else if (*ident == "ifndef") {
							auto param = GetParameters<Ident>(row);
							param ? doDirective(&Preprocessor::doIfndef, param.get<0>()) :
								throw AMKd::Utility::SyntaxException {MISSING("#ifndef")};
						}
						else if (*ident == "endif")
							doDirective(&Preprocessor::doEndif);
						else if (*ident == "if")
							doDirective(&Preprocessor::doIf, parsePredicate(row));
						else if (*ident == "elseif")
							doDirective(&Preprocessor::doElseIf, parsePredicate(row));
						else if (*ident == "else")
							doDirective(&Preprocessor::doElse);
						else if (*ident == "include") {
							auto param = GetParameters<Sep<'\"'>, QString>(row);
							throw AMKd::Utility::SyntaxException {"TODO: Implement #include"};
						}
						else if (*ident == "error") {
							row.SkipSpaces();
							auto msg = *row.Trim("[^;]*");
							fatalError(!msg.empty() ? msg : "(empty #error directive)", filename, text.GetLineNumber());
						}
						else if (*ident == "amm")
							doDirective(&Preprocessor::doTarget, Target::AMM);
						else if (*ident == "am4")
							doDirective(&Preprocessor::doTarget, Target::AM4);
						else if (*ident == "amk") {
							auto param = GetParameters<Opt<Sep<'='>>, SInt>(row);
							param ? doDirective(&Preprocessor::doVersion, param.get<1>()) :
								throw AMKd::Utility::SyntaxException {MISSING("#amk")};
						}
						else
							add += '#' + spaces + *ident + *row.Trim(".*");
					else {
						allowDirectives = false;
						add += '#' + spaces;
						if (auto num = row.Trim("[0-7]+")) {
							add += *num;
							for (char ch : *num)
								firstChannel = std::min(firstChannel, ch - '0');
						}
						add += *row.Trim(".*");
					}
				}
				else if (row.Trim('\"')) {
					allowDirectives = false;
					if (auto param = Lexer::QString()(row))
						add += '\"' + *param + '\"';
					else
						throw AMKd::Utility::SyntaxException {"Error parsing replacement directive."};
				}
				else if (target != Target::AMM && row.Trim(';')) {
					if (row.Trim("\\s*title\\s*=\\s*") && row)
						title = *row.Trim(".*");
					break;
				}
				else {
					allowDirectives = false;
					add += *row.Trim(".");
				}
			}

			if (text.Trim('\n'))
				add += '\n';
			result += add;
		}

		if (okayStatus.size() > 1)
			throw AMKd::Utility::SyntaxException {"There was an #ifdef, #ifndef, or #if without a matching #endif."};
	}
	catch (AMKd::Utility::SyntaxException &e) {
		fatalError(e.what(), filename, text.GetLineNumber());
	}
}

void Preprocessor::doDefine(const std::string &str, int val) {
	if (okayStatus.top())
		if (!defines.insert(std::make_pair(str, val)).second)
			throw AMKd::Utility::SyntaxException {"Preprocessor constant redefinition."};
}

void Preprocessor::doUndef(const std::string &str) {
	if (okayStatus.top())
		defines.erase(str);
}

void Preprocessor::doIfdef(const std::string &str) {
	doIf(defines.find(str) != defines.cend());
}

void Preprocessor::doIfndef(const std::string &str) {
	doIf(defines.find(str) == defines.cend());
}

void Preprocessor::doEndif() {
	okayStatus.pop();
	if (okayStatus.empty())
		throw AMKd::Utility::SyntaxException {"Cannot use this directive outside an #if, #ifdef, or #ifndef block."};
}

void Preprocessor::doIf(bool enable) {
	okayStatus.push(okayStatus.top() && enable);
}

void Preprocessor::doElseIf(bool enable) {
	doEndif();
	doIf(enable);
}

void Preprocessor::doElse() {
	bool prev = okayStatus.top();
	doEndif();
	doIf(!prev);
}

void Preprocessor::doTarget(Target t) {
	target = t;
	version = 0;
}

void Preprocessor::doVersion(int ver) {
	target = Target::AMK;
	version = ver;
}

bool Preprocessor::parsePredicate(SourceFile &row) {
	auto param = Lexer::GetParameters<Lexer::Ident, Comp, Lexer::SInt>(row);
	if (!param)
		throw AMKd::Utility::SyntaxException {"Missing argument for #if directive."};
	auto it = std::as_const(defines).find(param.get<0>());
	bool enable = (it != defines.cend());
	if (enable) {
		auto lhs = it->second;
		auto rhs = param.get<2>();
		switch (param.get<1>()) {
		case compare_t::EQ: enable = (lhs == rhs); break;
		case compare_t::NE: enable = (lhs != rhs); break;
		case compare_t::LT: enable = (lhs < rhs); break;
		case compare_t::LE: enable = (lhs <= rhs); break;
		case compare_t::GT: enable = (lhs > rhs); break;
		case compare_t::GE: enable = (lhs >= rhs); break;
		}
	}
	return enable;
}
