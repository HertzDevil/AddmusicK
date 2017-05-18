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
	void InsertData(std::vector<uint8_t> &buf) const;

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

// // //
const auto replacementComp = [] (const std::string &a, const std::string &b) {
	size_t al = a.length(), bl = b.length();
	return std::tie(al, b) > std::tie(bl, a);
};

class Music
{


public:
	double introSeconds;
	double mainSeconds;

	int noteParamaterByteCount;

	int tempoRatio;
	int divideByTempoRatio(int, bool fractionIsError);		// Divides a value by tempoRatio.  Errors out if it can't be done without a decimal (if the parameter is set).
	int multiplyByTempoRatio(int);					// Multiplies a value by tempoRatio.  Errors out if it goes higher than 255.
	bool nextHexIsArpeggioNoteLength;	

	std::string name;
	const std::string &getFileName() const;
	std::string pathlessSongName;
	bool playOnce;
	bool hasIntro;
	std::map<int, uint16_t> loopPointers;		// // //
	//unsigned int loopLengths[0x10000];		// How long, in ticks, each loop is.
	std::string text;
	std::vector<std::pair<std::string, std::string>> macroRecord_;		// // //
	AMKd::MML::SourceFile mml_;		// // //
	size_t totalSize;		// // //
	int spaceForPointersAndInstrs;
	std::vector<uint8_t> allPointersAndInstrs;		// // //
	std::vector<uint8_t> instrumentData;
	std::vector<uint8_t> finalData;

	SpaceInfo spaceInfo;

	Track tracks[CHANNELS + 1];		// // //
	
	unsigned int introLength;
	unsigned int mainLength;

	unsigned int seconds;

	bool hasYoshiDrums;

	bool knowsLength;

	int index;

	//byte mySamples[255];
	std::vector<unsigned short> mySamples;
	//int mySampleCount;
	int echoBufferSize;

	std::string statStr;

	std::string title;
	std::string author;
	std::string game;
	std::string comment;

	bool usedSamples[256];		// Holds a record of which samples have been used for this song.

	size_t minSize;		// // //

	bool exists;

	int posInARAM;

	void compile();

	size_t getDataSize() const;		// // //
	void adjustLoopPointers();		// // //

	// // //
	bool inRemoteDefinition;
	//int remoteDefinitionArg;

	std::map<std::string, std::string, decltype(replacementComp)> replacements {replacementComp};		// // //
	Music();

	void init();
private:
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
	void parseT();
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

	template <typename... Args>		// // //
	void append(Args&&... value);
	bool trimChar(char c);		// // //
	bool trimDirective(std::string_view str);		// // //
	void skipChars(size_t count);		// // //
	void skipSpaces();		// // //

	bool pushReplacement(const std::string &key, const std::string &value);		// // //
	bool popReplacement();		// // //
	bool doReplacement();		// // //
	bool hasNextToken();		// // //
	int peek();		// // //

	int getInt();
	int getInt(const std::string &str, int &p);
	int getIntWithNegative();
	int getHex(bool anyLength = false);
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
	void handleSuperLoopEnter();					// Call any time a definition of a super loop is entered.
	void handleNormalLoopRemoteCall(int loopCount);			// Call any time a normal loop is called remotely.
	void handleNormalLoopExit(int loopCount);			// Call any time a definition of a loop is exited.
	void handleSuperLoopExit(int loopCount);			// Call any time a definition of a super loop is exited.

	void addNoteLength(double ticks);				// Call this every note.  The correct channel/loop will be automatically updated.
};
