#pragma once		// // //

#include <vector>
#include <string>
#include <map>
#include <cstdint>		// // //
#include "MML/SourceFile.h"		// // //

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

// // // eventually replace this with AMKd::Music::Track
class Track
{
public:
	friend class Music;

private:
	std::vector<uint8_t> data;		// // //
	std::vector<uint16_t> loopLocations; // With remote loops, we can have remote loops in standard loops, so we need that ninth channel.
	std::vector<std::pair<uint16_t, std::vector<uint8_t>>> remoteGainInfo;		// // // holds position and remote call data for gain command conversions

	double channelLength = 0.; // How many ticks are in each channel.
	int q = 0x7F;
	int instrument = 0;
	//uint8_t lastFAGainValue = 0;
	//uint8_t lastFADelayValue = 0;
	//uint8_t lastFCGainValue = 0;
	uint8_t lastFCDelayValue = 0;

	unsigned short phrasePointers[2] = {0, 0}; // first 8 only
	bool noMusic[2] = {false, false}; // first 8 only
	bool passedIntro = false; // first 8 only
	bool updateQ = true;
	bool usingFA = false;
	bool usingFC = false;
	bool ignoreTuning = false; // Used for AM4 compatibility.  Until an instrument is explicitly declared on a channel, it must not use tuning.
	bool isDefiningLabelLoop = false;		// // //
};

class Music
{
	double introSeconds;
	double mainSeconds;

	int tempoRatio;
	int divideByTempoRatio(int, bool fractionIsError);		// Divides a value by tempoRatio.  Errors out if it can't be done without a decimal (if the parameter is set).
	int multiplyByTempoRatio(int);					// Multiplies a value by tempoRatio.  Errors out if it goes higher than 255.

public:		// // //
	std::string name;
public:
	const std::string &getFileName() const;

private:		// // //
	bool hasIntro;
	std::map<int, uint16_t> loopPointers;		// // //
	//unsigned int loopLengths[0x10000];		// How long, in ticks, each loop is.
	AMKd::MML::SourceFile mml_;		// // //

//public:		// // //
	Track tracks[CHANNELS + 1];		// // //

	unsigned int introLength;
	unsigned int mainLength;

	bool knowsLength;

public:		// // //
	size_t getDataSize() const;		// // //
	void FlushSongData(std::vector<uint8_t> &buf) const;		// // //
	void adjustLoopPointers();		// // //

	SpaceInfo spaceInfo;
	std::vector<uint8_t> instrumentData;
	std::vector<uint8_t> finalData;
	std::vector<uint8_t> allPointersAndInstrs;		// // //
	std::vector<unsigned short> mySamples;
	size_t totalSize;		// // //
	size_t minSize;		// // //
	int posInARAM;
	int spaceForPointersAndInstrs;
	int echoBufferSize;
	bool exists;
	bool hasYoshiDrums;

	int index;
	std::string title;
	std::string author;
	std::string game;
	std::string comment;
	unsigned int seconds;

private:
	std::string statStr;

	bool usedSamples[256];		// Holds a record of which samples have been used for this song.

	// // //
	bool inRemoteDefinition;
	//int remoteDefinitionArg;

public:		// // //
	Music();
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
	void parseOpenParenCommand();
	void parseLabelLoopCommand();
	void parseLoopCommand();
	void parseLoopEndCommand();
	void parseStarLoopCommand();
	void parseVibratoCommand();
	void parseTripletOpenDirective();
	void parseTripletCloseDirective();
	void parseRaiseOctaveDirective();
	void parseLowerOctaveDirective();
	void parsePitchSlideCommand();
	void parseHexCommand();
	void parseNote(int ch);		// // //
	void parseHDirective();
	void parseReplacementDirective();
	void parseNCommand();

	void parseOptionDirective();
	
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

	void parseDefine();
	void parseIfdef();
	void parseIfndef();
	void parseEndif();
	void parseUndef();

	void parseSPCInfo();

	// // // action methods, these will become objects later
	void doVolume(int vol);
	void doGlobalVolume(int vol);
	void doVibrato(int delay, int rate, int depth);
	void doTremolo(int delay, int rate, int depth);
	void doTremoloOff();
	void doTempo(int speed);
	void doSampleLoad(int index, int mult);
	void doSubloopEnter();		// // // Call any time a definition of a super loop is entered.
	void doSubloopExit(int loopCount);		// // // Call any time a definition of a super loop is exited.
	void doVolumeTable(bool louder);

	template <typename... Args>		// // //
	void append(Args&&... value);
	bool trimChar(char c);		// // //
	bool trimDirective(std::string_view str);		// // //
	void skipSpaces();		// // //

	bool hasNextToken();		// // //
	int peek();		// // //

	int getInt();
	int getIntWithNegative();
	int getHex();		// // //
	bool getHexByte(int &out);		// // //
	int getPitch(int j);
	int getNoteLength();		// // //
	std::string getIdentifier();		// // //
	std::string getEscapedString();		// // //

	[[noreturn]] void fatalError(const std::string &str);		// // //

	//std::vector<std::string> defineStrings;

	void printChannelDataNonVerbose(int);
	void parseHFDHex();
	void parseHFDInstrumentHack(int addr, int bytes);
	void insertedZippedSamples(const std::string &path);
	void insertRemoteConversion(std::vector<uint8_t> &&cmd, std::vector<uint8_t> &&conv);		// // //
	void removeRemoteConversion();		// // //

	bool guessLength;
	
	std::map<int, double> loopLengths;		// // // How many ticks are in each loop.
	double normalLoopLength;				// How many ticks were in the most previously declared normal loop.
	double superLoopLength;					// How many ticks were in the most previously declared super loop.
	std::vector<std::pair<double, int>> tempoChanges;	// Where any changes in tempo occur. A negative tempo marks the beginning of the main loop, if an intro exists.

	bool baseLoopIsNormal;
	bool baseLoopIsSuper;
	bool extraLoopIsNormal;
	bool extraLoopIsSuper;

	void handleNormalLoopEnter();					// Call any time a definition of a loop is entered.
	void handleNormalLoopRemoteCall(int loopCount);			// Call any time a normal loop is called remotely.
	void handleNormalLoopExit(int loopCount);			// Call any time a definition of a loop is exited.

	void addNoteLength(double ticks);				// Call this every note.  The correct channel/loop will be automatically updated.
};
