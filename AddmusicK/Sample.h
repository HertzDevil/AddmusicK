#pragma once		// // //

#include <string>
#include <vector>
#include <cstdint>

struct Sample
{
	std::string name;
	std::vector<uint8_t> data;
	uint16_t loopPoint = 0;		// // //
	bool exists = false;
	bool important = true;	// If a sample is important, then is should never be excluded.
							// Otherwise, it's only used if the song has an instrument or $E5/$F3 command that is using it.
};
