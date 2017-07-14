#include "MusicParser.h"
#include "../Music.h"
#include "Lexer.h"
#include "../Utility/Exception.h"
#include "../globals.h"
#include "../Utility/StaticTrie.h"

using namespace AMKd::MML;
using namespace AMKd::MML::Lexer;

namespace {
template <typename T>
struct TrieAdaptor
{
	void operator()(const std::string_view &, std::size_t, SourceView &file, ::Music &music) {
		return T()(file, music);
	}
};
template <typename T, char... Cs>
using Entry = AMKd::Utility::TrieEntry<TrieAdaptor<T>, Cs...>;
} // namespace

void MusicParser::operator()(SourceView &file, ::Music &music) {
	using AMKd::Utility::TrieEntry;
	using CMDS = AMKd::Utility::StaticTrie<
		Entry<Parser::Comment    , ';'>,
		Entry<Parser::Replacement, '"'>
	>;

	const auto doParse = [&] (std::string_view &sv) {
		return AMKd::Utility::ParseTrie(CMDS { }, sv, file, music);
	};

	while (file.HasNextToken()) {		// // // TODO: also call this for selected lexers
		try {
			if (!file.TryProcess(doParse) && !music.compileStep())		// // // TODO: remove
				throw AMKd::Utility::SyntaxException {"Unexpected character \"" + *file.Trim(".") + "\" found."};
		}
		catch (AMKd::Utility::MMLException &e) {
			::printError(e.what(), music.name, file.GetLineNumber());
		}
	}
}

void Parser::Comment::operator()(SourceView &file, ::Music &music) {
	(void)GetParameters<Row>(file);
	return music.doComment();
}

void Parser::Replacement::operator()(SourceView &file, ::Music &) {
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
