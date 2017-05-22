#pragma once

namespace AMKd::Music {

// // // common stuff for music and sound effects
class SongBase
{
public:
	std::string name;
	int index;
	int posInARAM;

	bool exists = false;
};

} // namespace AMKd::Music
