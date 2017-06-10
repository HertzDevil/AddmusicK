#pragma once		// // //

#include <vector>
#include <string>
#include <map>
#include <cstdint>		// // //
#include "MML/SourceFile.h"		// // //
#include "Music/SongBase.h"		// // //
#include "MML/Duration.h"		// // //
#include "Music/Track.h"		// // //

const size_t CHANNELS = 8;		// // //

struct SpaceInfo {
	int songStartPos;
	int songEndPos;
	int sampleTableStartPos;
	int sampleTableEndPos;
	std::vector<int> individualSampleStartPositions;
	std::vector<int> individualSampleEndPositions;
	std::vector<bool> individialSampleIsImportant;
	int importantSampleCount;
	int echoBufferEndPos;
	int echoBufferStartPos;

};

class Music : public AMKd::Music::SongBase		// // //
{
public:
	Music();
	
	const std::string &getFileName() const;		// // //

	size_t getDataSize() const;		// // //
	void FlushSongData(std::vector<uint8_t> &buf) const;		// // //
	void adjustLoopPointers();		// // //

	void compile();

private:
	void init();
	void pointersFirstPass();

	void parseComment();
	void parseQMarkDirective();
	void parseExMarkDirective();
	void parseChannelDirective();
	void parseLDirective();
	void parseGlobalVolumeCommand();
	void parseVolumeCommand();
	void parseQuantizationCommand();
	void parsePanCommand();
	void parseIntroDirective();
	// // //
	void parseTempoCommand();
	void parseTransposeDirective();
	void parseOctaveDirective();
	void parseInstrumentCommand();
	void parseDirectInstCommand();		// // //
	void parseOpenParenCommand();
	void parseLabelLoopCommand();
	void parseRemoteCodeCommand();		// // //
	void parseLoopCommand();
	void parseSubloopCommand();		// // //
	void parseErrorLoopCommand();		// // //
	void parseLoopEndCommand();
	void parseSubloopEndCommand();		// // //
	void parseErrorLoopEndCommand();		// // //
	void parseStarLoopCommand();
	void parseVibratoCommand();
	void parseTripletOpenDirective();
	void parseTripletCloseDirective();
	void parseRaiseOctaveDirective();
	void parseLowerOctaveDirective();
	void parseHexCommand();
	void parseHDirective();
	void parseReplacementDirective();
	void parseNCommand();
	void parseBarDirective();		// // //
	
	void parseNote(int note);		// // //
	void parseNoteCommon(int offset);
	void parseNoteC();
	void parseNoteD();
	void parseNoteE();
	void parseNoteF();
	void parseNoteG();
	void parseNoteA();
	void parseNoteB();
	void parseTie();
	void parseRest();

	void parseOptionDirective();

	// // //
	void parseSMWVTable();
	void parseNSPCVTable();
	void parseTempoImmunity();
	void parseNoLoop();
	void parseDivideTempo();
	void parseHalveTempo();

	void parseSpecialDirective();
	void parseInstrumentDefinitions();
	void parseSampleDefinitions();
	void parsePadDefinition();
	void parseASMCommand();
	void parseJSRCommand();
	void parseLouderCommand();
	void parseTempoImmunityCommand();
	void parsePath();
	void compileASM();

	void parseSPCInfo();

	// // // action methods, these will become objects later
	void doNote(int note, int fullTicks, int bendTicks, bool nextPorta);
	void doOctave(int oct);
	void doRaiseOctave();
	void doLowerOctave();
	void doVolume(int vol);
	void doGlobalVolume(int vol);
	void doInstrument(int inst, bool add);
	void doVibrato(int delay, int rate, int depth);
	void doTremolo(int delay, int rate, int depth);
	void doTremoloOff();
	void doEnvelope(int ad, int sr);
	void doTempo(int speed);
	void doSampleLoad(int index, int mult);
	void doLoopEnter();					// Call any time a definition of a loop is entered.
	void doLoopExit(int loopCount);			// Call any time a definition of a loop is exited.
	void doLoopRemoteCall(int loopCount, uint16_t loopAdr);		// // // Call any time a normal loop is called remotely.
	void doSubloopEnter();		// // // Call any time a definition of a super loop is entered.
	void doSubloopExit(int loopCount);		// // // Call any time a definition of a super loop is exited.
	void doVolumeTable(bool louder);

	template <typename... Args>		// // //
	void append(Args&&... value);

	AMKd::Music::Track &getActiveTrack();		// // //
	const AMKd::Music::Track &getActiveTrack() const;		// // //
	AMKd::Music::Track &getBaseTrack();		// // //

	int getPitch(int j);
	int getRawTicks(const AMKd::MML::Duration &dur) const;		// // //
	int getFullTicks(const AMKd::MML::Duration &dur) const;		// // //
	int getLastTicks(const AMKd::MML::Duration &dur) const;		// // //
	int checkTickFraction(double ticks) const;		// // //

	[[noreturn]] void fatalError(const std::string &str);		// // //

	void printChannelDataNonVerbose(int size);
	void parseHFDHex();
	void parseHFDInstrumentHack(int addr, int bytes);
	void insertedZippedSamples(const std::string &path);
	void insertRemoteConversion(uint8_t cmdtype, uint8_t param, std::vector<uint8_t> &&cmd);		// // //

	void addNoteLength(double ticks);				// Call this every note.  The correct channel/loop will be automatically updated.
	void writeState(AMKd::Music::TrackState (AMKd::Music::Track::*state), int val);		// // //
	void resetStates();		// // //
	void synchronizeStates();		// // //

	int divideByTempoRatio(int, bool fractionIsError);		// Divides a value by tempoRatio.  Errors out if it can't be done without a decimal (if the parameter is set).
	int multiplyByTempoRatio(int);					// Multiplies a value by tempoRatio.  Errors out if it goes higher than 255.

public:		// // //
	SpaceInfo spaceInfo;
	std::vector<uint8_t> instrumentData;
	std::vector<uint8_t> finalData;
	std::vector<uint8_t> allPointersAndInstrs;		// // //
	std::vector<unsigned short> mySamples;
	size_t totalSize;		// // //
	size_t minSize;		// // //
	int instrumentPos;		// // //
	int echoBufferSize;
	bool hasYoshiDrums;

	std::string title;
	std::string author;
	std::string game;
	std::string comment;
	std::string dumper;		// // //
	unsigned int seconds;

private:
	double introSeconds;
	double mainSeconds;

	int tempoRatio;

	bool hasIntro;
	std::map<int, uint16_t> loopPointers;		// // //
	//unsigned int loopLengths[0x10000];		// How long, in ticks, each loop is.
	AMKd::MML::SourceFile mml_;		// // //

	AMKd::Music::Track tracks[CHANNELS];		// // //
	AMKd::Music::Track loopTrack;		// // //

	unsigned int introLength;
	unsigned int mainLength;

	bool knowsLength;

private:
	std::string statStr;

	bool usedSamples[256] = { };		// // // Holds a record of which samples have been used for this song.

	// // //
	bool inRemoteDefinition;
	//int remoteDefinitionArg;

	bool guessLength;
	
	std::map<int, double> loopLengths;		// // // How many ticks are in each loop.
	double normalLoopLength;				// How many ticks were in the most previously declared normal loop.
	double superLoopLength;					// How many ticks were in the most previously declared super loop.
	std::vector<std::pair<double, int>> tempoChanges;	// Where any changes in tempo occur. A negative tempo marks the beginning of the main loop, if an intro exists.

	enum class LoopType { none, normal, sub };		// // //
	LoopType loopState1 = LoopType::none, loopState2 = LoopType::none;
};
