#pragma once		// // //

#include <vector>
#include <string>
#include <map>
#include <cstdint>		// // //
#include "MML/SourceView.h"		// // //
#include "Music/SongBase.h"		// // //
#include "Music/Track.h"		// // //

namespace AMKd::MML {
struct Duration;
}
namespace AMKd::Binary {
class Stream;
}

const size_t CHANNELS = 8;		// // //

struct SpaceInfo {
	int songStartPos;
	int songEndPos;
	int sampleTableStartPos;
	int sampleTableEndPos;
	struct SampleInfo		// // //
	{
		int startPosition;
		int endPosition;
		bool important;
	};
	std::vector<SampleInfo> individualSamples;
	int importantSampleCount;
	int echoBufferStartPos;
	int echoBufferEndPos;

};

class Music : public AMKd::Music::SongBase		// // //
{
public:
	Music();
	
	const std::string &getFileName() const;		// // //

	size_t getDataSize() const;		// // //
	std::vector<uint8_t> getSongData() const;		// // //
	void adjustHeaderPointers();		// // //
	void adjustLoopPointers();		// // //

	void init();
	void compile();
	bool compileStep();		// // // TODO: remove
	void pointersFirstPass();
	void displaySongData() const;		// // //

private:
	void parseQMarkDirective();
	void parseChannelDirective();
	void parseLDirective();
	void parseGlobalVolumeCommand();
	void parseVolumeCommand();
	void parseQuantizationCommand();
	void parsePanCommand();
	// // //
	void parseTempoCommand();
	void parseTransposeDirective();
	void parseOctaveDirective();
	void parseInstrumentCommand();
	void parseDirectInstCommand();		// // //
	void parseOpenParenCommand();
	void parseRemoteCodeCommand();		// // //
	void parseLoopEndCommand();
	void parseSubloopEndCommand();		// // //
	void parseStarLoopCommand();
	void parseVibratoCommand();
	void parseHexCommand();
	void parseHDirective();
	void parseNCommand();
	
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

public:
	// // // action methods, these will become objects later
	void doDirectWrite(int byte);
	void doComment();
	void doBar();
	void doExMark();
	void doNote(int note, int fullTicks, int bendTicks, bool nextPorta);
	void doOctave(int oct);
	void doRaiseOctave();
	void doLowerOctave();
	void doVolume(int vol);
	void doVolume(int vol, int fadeLen);
	void doGlobalVolume(int vol);
	void doGlobalVolume(int vol, int fadeLen);
	void doPan(int pan, bool sLeft, bool sRight);
	void doPanFade(int pan, int fadeLen);
	void doEchoVolume(int left, int right, int mask);
	void doEchoParams(int delay, int reverb, int firIndex);
	void doEchoFade(int left, int right, int fadeLen);
	void doEchoOff();
	void doEchoToggle();
	void doInstrument(int inst, bool add);
	void doVibrato(int delay, int rate, int depth);
	void doVibratoOff();
	void doVibratoFade(int fadeLen);
	void doTremolo(int delay, int rate, int depth);
	void doTremoloOff();
	void doPortamento(int delay, int len, int target);
	void doPitchBend(int delay, int len, int target, bool bendAway);
	void doDetune(int offset);
	void doEnvelope(int ad, int sr);
	void doFIRFilter(int f0, int f1, int f2, int f3, int f4, int f5, int f6, int f7);
	void doNoise(int pitch);
	void doTempo(int speed);
	void doTempo(int speed, int fadeLen);
	void doTranspose(int offset);
	void doTransposeGlobal(int offset);
	void doSampleLoad(int index, int mult);
	void doLoopEnter();					// Call any time a definition of a loop is entered.
	void doLoopExit(int loopCount);			// Call any time a definition of a loop is exited.
	void doLoopLabel(int label);
	void doLoopRemoteCall(int loopCount, uint16_t loopAdr);		// // // Call any time a normal loop is called remotely.
	void doLoopNative(int addr, int loopCount);
	void doSubloopEnter();		// // // Call any time a definition of a super loop is entered.
	void doSubloopExit(int loopCount);		// // // Call any time a definition of a super loop is exited.
	void doRemoteCallNative(int addr, int type, int delay = 0);
	void doIntro();
	void doYoshiDrums(bool ch5only);
	void doLegato();
	void doLightStaccato();
	void doSync();
	void doTempoImmunity();
	void doVolumeTable(bool louder);
	void doRestoreInst();
	void doPitchMod(int flag);
	void doGain(int gain);
	void doAmplify(int mult);
	void doEchoBuffer(int size);
	void doDSPWrite(int adr, int val);
	void doARAMWrite(int adr, int val);
	void doDataSend(int val1, int val2);
	void doArpeggio(int dur, const std::vector<uint8_t> &notes);
	void doTrill(int dur, int offset);
	void doGlissando(int dur, int offset);
	void doTriplet(bool enable);

private:
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

	void warn(const std::string &str) const;		// // //

	void parseHFDHex();
	void parseHFDInstrumentHack(int addr, int bytes);
	void insertedZippedSamples(const std::string &path);
	void insertRemoteConversion(uint8_t cmdtype, uint8_t param, const AMKd::Binary::Stream &cmd);		// // //
	void insertRemoteConversion(uint8_t cmdtype, uint8_t param, AMKd::Binary::Stream &&cmd);		// // //

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
	size_t headerSize = 0;		// // //
	size_t totalSize = 0;		// // //
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

	std::string mmlText_;		// // //
	AMKd::MML::SourceView mml_;		// // //

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
