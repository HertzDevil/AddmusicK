#pragma once

#include <cstdint>		// // //

namespace AMKd::Music {

// // // common stuff for music and sound effects
class SongBase
{
public:
	std::string name;
	int index;
	uint16_t posInARAM;

	bool exists = false;
};

} // namespace AMKd::Music
