#pragma once		// // //

#include <string>
#include <vector>
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
	int getHex();
	int getInt();
	int getPitch(int i, int octave);
	int getNoteLength(int i);

	void parseASM();
	void compileASM();
	void parseJSR();

	void parseDefine();
	void parseIfdef();
	void parseIfndef();
	void parseEndif();
	void parseUndef();
	
	std::vector<std::string> defineStrings;

	std::vector<std::string> asmStrings;
	std::vector<std::string> asmNames;
	std::vector<std::string> jmpNames;
	std::vector<int> jmpPoses;
};
