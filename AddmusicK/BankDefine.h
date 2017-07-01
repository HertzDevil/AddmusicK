#pragma once		// // //

#include <string>
#include <vector>

struct BankDefine
{
	std::string bankName;		// // //
	struct Sample
	{
		std::string name;
		bool important;
	};
	std::vector<Sample> samples;		// // //
};
