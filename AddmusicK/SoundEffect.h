#pragma once		// // //

#include <string>
#include <vector>
#include <map>		// // //
#include <cstdint>
#include "Music/SongBase.h"		// // //
#include "MML/SourceView.h"		// // //
#include "Binary/Stream.h"		// // //

class SoundEffect : public AMKd::Music::SongBase		// // //
{
public:
	const std::string &getEffectiveName() const;		// Returns name or pointName.
	void loadMML(const std::string &mml);		// // //
	void compile();

	int bank;
	AMKd::Binary::Stream dataStream;		// // //
	std::vector<uint8_t> code;		// // //
	bool add0 = true;		// // //
	std::string pointName;
	int pointsTo = 0;		// // //

private:		// // //
	template <typename... Args>		// // //
	void append(Args&&... value);

	int getPitch(int i, int octave);
	int getNoteLength();

	void parseStep();		// // //
	void parseASM();
	void compileASM();
	void parseJSR();

	bool triplet;		// // //
	int defaultNoteValue;		// // //

	AMKd::MML::SourceView mml_;		// // //
	std::string mmlText_;		// // //
	std::map<std::string, std::string> asmInfo;		// // //
	std::vector<std::pair<std::string, int>> jmpInfo;		// // //
};
