#include "MusicParser.h"
#include "../Music.h"
#include "Tokenizer.h"
#include "Lexer.h"
#include "../Utility/Exception.h"
#include "../globals.h"

using namespace AMKd::MML;
using namespace AMKd::MML::Lexer;

void MusicParser::compile(SourceView &file, ::Music &music) {
	using parse_func_t = decltype(&MusicParser::compile);
	static const AMKd::Utility::Trie<parse_func_t> CMDS {		// // //
		{"\"", &MusicParser::parseReplacementDirective},
		{";", &MusicParser::parseComment},
	};

	Tokenizer tok;

	while (file.HasNextToken()) {
		try {		// // // TODO: also call this for selected lexers
			if (auto token = tok(file, CMDS))
				(this->*(*token))(file, music);
			else if (!music.compileStep())		// // // TODO: remove
				throw AMKd::Utility::SyntaxException {"Unexpected character \"" + *file.Trim(".") + "\" found."};
		}
		catch (AMKd::Utility::MMLException &e) {
			::printError(e.what(), music.name, file.GetLineNumber());
		}
	}
}

void MusicParser::parseComment(SourceView &file, ::Music &music) {
	(void)GetParameters<Row>(file);
	return music.doComment();
}

void MusicParser::parseReplacementDirective(SourceView &file, ::Music &) {
	file.Unput();
	auto param = GetParameters<QString>(file);
	if (!param)
		throw AMKd::Utility::SyntaxException {"Unexpected end of replacement directive."};
	std::string s = param.get<0>();
	size_t i = s.find('=');
	if (i == std::string::npos)
		throw AMKd::Utility::SyntaxException {"Error parsing replacement directive; could not find '='"};		// // //

	std::string findStr = s.substr(0, i);
	std::string replStr = s.substr(i + 1);

	while (!findStr.empty() && isspace(findStr.back()))
		findStr.pop_back();
	if (findStr.empty())
		throw AMKd::Utility::ParamException {"Error parsing replacement directive; string to find was of zero length."};

	while (!replStr.empty() && isspace(replStr.front()))
		replStr.erase(0, 1);

	file.AddMacro(findStr, replStr);		// // //
}
