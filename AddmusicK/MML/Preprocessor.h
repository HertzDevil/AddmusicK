#pragma once

#include <string>
#include <stack>
#include <map>

namespace AMKd::MML {

class SourceFile;

class Preprocessor
{
public:
	Preprocessor(const std::string &str, const std::string &filename);

	std::string result;
	std::string title;
	int version;
	int firstChannel;

private:
	void doDefine(const std::string &str, int val);
	void doUndef(const std::string &str);
	void doIfdef(const std::string &str);
	void doIfndef(const std::string &str);
	void doEndif();
	void doIf(bool enable);
	void doElseIf(bool enable);		// // //
	void doElse();		// // //

	void doVersion(int ver);		// // //

	bool parsePredicate(SourceFile &row);		// // //

	std::map<std::string, int> defines;
	std::stack<bool> okayStatus;
};

} // namespace AMKd::MML
