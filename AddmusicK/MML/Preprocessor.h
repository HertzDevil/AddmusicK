#pragma once

#include <string>
#include <string_view>
#include <stack>
#include <map>

namespace AMKd::MML {

enum class Target		// // //
{
	Unknown, AMK, AM4, AMM,
};

class SourceView;

class Preprocessor
{
public:
	Preprocessor(std::string_view str, const std::string &filename);

	std::string result;
	std::string title;
	Target target;		// // //
	int version;
	int firstChannel;

private:
	void doDefine(std::string_view str, int val);
	void doUndef(std::string_view str);
	void doIfdef(std::string_view str);
	void doIfndef(std::string_view str);
	void doEndif();
	void doIf(bool enable);
	void doElseIf(bool enable);		// // //
	void doElse();		// // //

	void doTarget(Target t);		// // //
	void doVersion(int ver);		// // //

	bool parsePredicate(SourceView &row);		// // //

	std::map<std::string_view, int> defines;
	std::stack<bool> okayStatus;
};

} // namespace AMKd::MML
