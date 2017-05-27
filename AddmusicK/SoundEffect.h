#pragma once		// // //

#include <string>
#include <vector>
#include <map>		// // //
#include <cstdint>
#include "Music/SongBase.h"		// // //

class SoundEffect : public AMKd::Music::SongBase		// // //
{
public:
	std::string &getEffectiveName();		// Returns name or pointName.
	void compile();

	int bank;
	std::vector<uint8_t> data;		// // //
	std::vector<uint8_t> code;		// // //
	std::string text;
	bool add0 = true;		// // //
	std::string pointName;
	int pointsTo = 0;		// // //

private:		// // //
	template <typename... Args>		// // //
	void append(Args&&... value);

	int getHex();
	int getInt();
	int getPitch(int i, int octave);
	int getNoteLength(int i);

	void skipSpaces();		// // //

	void parseASM();
	void compileASM();
	void parseJSR();

	std::map<std::string, std::string> asmInfo;		// // //
	std::vector<std::pair<std::string, int>> jmpInfo;		// // //
};
