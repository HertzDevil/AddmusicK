#include "Music.h"
//#include "globals.h"
//#include "Sample.h"

#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <locale>		// // //

#include "globals.h"		// // //
#include "Binary/ChunkAMK.h"		// // //
#include "Binary/Stream.h"		// // //
#include "MML/Lexer.h"		// // //
#include "MML/Preprocessor.h"		// // //
#include "MML/Tokenizer.h"		// // //
#include "MML/MusicParser.h"		// // //
#include "Utility/Exception.h"		// // //
#include <functional>

using Track = AMKd::Music::Track;		// // //



// // //
#define CMD_ERROR(name, abbr) "Error parsing " name " (\"" abbr "\") command."
#define CMD_ILLEGAL(name, abbr) "Illegal value for " name " (\"" abbr "\") command."
#define DIR_ERROR(name) "Error parsing " name " directive."
#define DIR_ILLEGAL(name) "Illegal value for " name " directive."

template <typename T, typename U>
T requires(T x, T min, T max, U&& err) {
	return (x >= min && x <= max) ? x :
		throw AMKd::Utility::ParamException {std::forward<U>(err)};
}

// // //
void Music::warn(const std::string &str) const {
	::printWarning(str, name, mml_.GetLineNumber());		// // //
}

// // //
static void downcase(std::string &str) {
	std::use_facet<std::ctype<char>>(std::locale()).tolower(&str[0], &str[0] + str.size());
}

static int channel, prevChannel;
static bool inNormalLoop;		// // //
static bool inSubLoop;			// Whether or not we're in an $E6 loop.

static uint16_t prevLoop;		// // //
static bool doesntLoop;

using AMKd::MML::Target;		// // //
static Target songTargetProgram = Target::Unknown;
static int targetAMKVersion;

static int loopLabel = 0;

static const int tmpTrans[19] = { 0, 0, 5, 0, 0, 0, 0, 0, 0, -5, 6, 0, -5, 0, 0, 8, 0, 0, 0 };
static const int instrToSample[30] = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x07, 0x08, 0x09, 0x05, 0x0A,	// \ Instruments
0x0B, 0x01, 0x10, 0x0C, 0x0D, 0x12, 0x0C, 0x11, 0x01,		// /
0x00, 0x00,							// Nothing
0x0F, 0x06, 0x06, 0x0E, 0x0E, 0x0B, 0x0B, 0x0B, 0x0E };		// Percussion

static const int hexLengths[] = {
	                              2, 2, 3, 4, 4, 1,
	2, 3, 2, 3, 2, 4, 2, 2, 3, 4, 2, 4, 4, 3, 2, 4,
	1, 4, 4, 3, 2, 9, 3, 4, 2, 3, 3, 2, 5
};
static int transposeMap[256];
//static bool htranspose[256];

//static int tempLoopLength;		// How long the current [ ] loop is.
//static int e6LoopLength;		// How long the current $E6 loop is.
//static int previousLoopLength;	// How long the last encountered loop was.
/*
static int loopNestLevel;		// How deep we're "loop nested".
// If this is 0, then any type of loop is allowed.
// If this is 1 and we're in a normal loop, then normal loops are disallowed and $E6 loops are allowed.
// If this is 1 and we're in an $E6 loop, then $E6 loops are disallowed and normal loops are allowed.
// If this is 2, then no new loops are allowed.
*/

//static unsigned int lengths[CHANNELS];		// How long each channel is.

static unsigned int tempo;
//static bool onlyHadOneTempo;

static bool manualNoteWarning;

static bool channelDefined;
//static int am4silence;			// Used to keep track of the brief silence at the start of am4 songs.

//static bool normalLoopInsideE6Loop;
//static bool e6LoopInsideNormalLoop;

static fs::path basepath;		// // //

static bool usingSMWVTable;



// // //
template <typename... Args>
void Music::append(Args&&... value) {
	getActiveTrack().append(std::forward<Args>(value)...);
}

Music::Music() {
	knowsLength = false;
	echoBufferSize = 0;
	for (size_t i = 0; i < std::size(tracks); ++i)		// // //
		tracks[i].index = i;
}

void Music::init() {
	basepath = ".";		// // //
	manualNoteWarning = true;
	//am4silence = 0;
	// // //
	hasYoshiDrums = false;
	//onlyHadOneTempo = true;
	tempo = 0x36;
	guessLength = true;

	channelDefined = false;
	tempoRatio = 1;

	// // //
	//remoteDefinitionArg = 0;
	inRemoteDefinition = false;
	inNormalLoop = false;		// // //
	inSubLoop = false;

	superLoopLength = normalLoopLength = 0;

	// // //

	seconds = 0;

	// // //

	hasIntro = false;
	doesntLoop = false;

	loopLabel = 0;
	// // //

	for (int z = 0; z < 19; z++)		// // //
		transposeMap[z] = tmpTrans[z];
	for (int z = 19; z < 256; z++)
		transposeMap[z] = 0;

	const auto stat = AMKd::MML::Preprocessor {openTextFile(fs::path {"music"} / name), name};
	mmlText_ = std::move(stat.result);		// // //
	mml_ = AMKd::MML::SourceView {mmlText_};
	songTargetProgram = stat.target;		// // //
	targetAMKVersion = stat.version;

	if (!stat.title.empty())
		title = stat.title;
	else
		title = fs::path {name}.stem().string();		// // //
	game = "Super Mario World (custom)";		// // //
	dumper = "AddmusicK " + std::to_string(AMKVERSION) + '.' + std::to_string(AMKMINOR) + '.' + std::to_string(AMKREVISION);		// // //

	switch (stat.target) {		// // //
	case Target::AMM:
		break;
	case Target::AM4:
		for (Track &t : tracks)
			t.ignoreTuning = true; // AM4 fix for tuning[] related stuff.
		loopTrack.ignoreTuning = true;		// // //
		break;
	case Target::AMK:
		/*
		targetAMKVersion = 0;
		writeTextFile(fs::path {"music"} / name, [&] { return mmltext + "\n\n#amk=1\n"; });
		*/
		if (targetAMKVersion > PARSER_VERSION)
			throw AMKd::Utility::MMLException {"This song was made for a newer version of AddmusicK.  You must update to use\nthis song."};
		break;
	default:
		throw AMKd::Utility::MMLException {"Song did not specify target program with #amk, #am4, or #amm."};
	}

	usingSMWVTable = (targetAMKVersion < 2);		// // //
	
	// // // We can't just insert this at the end due to looping complications and such.
	if (validateHex && index > highestGlobalSong && stat.firstChannel != CHANNELS) {
		channel = stat.firstChannel;
		if (!usingSMWVTable)
			doVolumeTable(true);
		doEchoBuffer(echoBufferSize);
	}

	prevChannel = channel = 0;
}

void Music::compile() {
	AMKd::MML::MusicParser()(mml_, *this);		// // //
}

// // //
bool Music::compileStep() {
	static const AMKd::Utility::Trie<void (Music::*)()> CMDS {		// // //
		{"#", &Music::parseChannelDirective},
		{"$", &Music::parseHexCommand},
		{"(", &Music::parseOpenParenCommand},
		{"(!", &Music::parseRemoteCodeCommand},
		{"*", &Music::parseStarLoopCommand},
		{"?", &Music::parseQMarkDirective},
		{"@", &Music::parseInstrumentCommand},
		{"@@", &Music::parseDirectInstCommand},
		{"a", &Music::parseNoteA},
		{"b", &Music::parseNoteB},
		{"c", &Music::parseNoteC},
		{"d", &Music::parseNoteD},
		{"e", &Music::parseNoteE},
		{"f", &Music::parseNoteF},
		{"g", &Music::parseNoteG},
		{"h", &Music::parseHDirective},
		{"l", &Music::parseLDirective},
		{"n", &Music::parseNCommand},
		{"o", &Music::parseOctaveDirective},
		{"p", &Music::parseVibratoCommand},
		{"q", &Music::parseQuantizationCommand},
		{"r", &Music::parseRest},
		{"t", &Music::parseTempoCommand},
		{"tuning", &Music::parseTransposeDirective},
		{"v", &Music::parseVolumeCommand},
		{"w", &Music::parseGlobalVolumeCommand},
		{"y", &Music::parsePanCommand},
		{"]", &Music::parseLoopEndCommand},
		{"]]", &Music::parseSubloopEndCommand},
		{"^", &Music::parseTie},
	};
	AMKd::MML::Lexer::Tokenizer tok;
	auto token = tok(mml_, CMDS);
	if (!token)
		return false;
	(this->*(*token))();
	return true;
}

// // //
size_t Music::getDataSize() const {
	size_t x = 0;
	for (const Track &t : tracks)
		x += t.GetStreamSize();
	return x;
}

// // //
std::vector<uint8_t> Music::getSongData() const {
	std::vector<uint8_t> buf;
	buf.reserve(getDataSize() + loopTrack.GetStreamSize() + allPointersAndInstrs.size());
	buf.insert(buf.end(), allPointersAndInstrs.cbegin(), allPointersAndInstrs.cend());
	for (const Track &t : tracks)
		t.FlushData(buf);
	loopTrack.FlushData(buf);
	return buf;
}

void Music::parseQMarkDirective() {
	using namespace AMKd::MML::Lexer;
	switch (GetParameters<Option<Int>>(mml_)->value_or(0)) {
	case 0: doesntLoop = true; return;
	case 1: getActiveTrack().noMusic[0] = true; return;		// // //
	case 2: getActiveTrack().noMusic[1] = true; return;
	}
	throw AMKd::Utility::ParamException {DIR_ILLEGAL("\"?\"")};
}

void Music::parseChannelDirective() {
	using namespace AMKd::MML::Lexer;		// // //
	if (auto param = GetParameters<Chan>(mml_)) {
		if (param->count() > 1)
			throw AMKd::Utility::MMLException {"TODO: Channel multiplexing"};
		for (size_t i = 0; i < CHANNELS; ++i)
			if (param.get<0>()[i]) {
				channel = i;
				resetStates();		// // //
				getActiveTrack().lastDuration = 0;
				getActiveTrack().usingH = false;
				channelDefined = true;
				/*
				for (int u = 0; u < CHANNELS * 2; ++u)
					if (htranspose[u])			// Undo what the h directive did.
						transposeMap[u] = tmpTrans[u];
				hTranspose = 0;
				*/
			}
		return;
	}

	parseSpecialDirective();
}

void Music::parseLDirective() {
	using namespace AMKd::MML::Lexer;		// // //
	if (auto param = GetParameters<Int>(mml_))
		return writeState(&Track::l, requires(param.get<0>(), 1u, 192u, DIR_ILLEGAL("note length (\"l\")")));		// // //
	throw AMKd::Utility::SyntaxException {DIR_ERROR("note length (\"l\")")};
}

void Music::parseGlobalVolumeCommand() {
	using namespace AMKd::MML::Lexer;		// // //
	if (auto param = GetParameters<Int>(mml_))
		return doGlobalVolume(requires(param.get<0>(), 0x00u, 0xFFu, CMD_ILLEGAL("global volume", "w")));		// // //
	throw AMKd::Utility::SyntaxException {CMD_ERROR("global volume", "w")};		// // //
}

void Music::parseVolumeCommand() {
	using namespace AMKd::MML::Lexer;		// // //
	if (auto param = GetParameters<Int>(mml_))
		return doVolume(requires(param.get<0>(), 0x00u, 0xFFu, CMD_ILLEGAL("volume", "v")));
	throw AMKd::Utility::SyntaxException {CMD_ERROR("volume", "v")};		// // //
}

void Music::parseQuantizationCommand() {
	using namespace AMKd::MML::Lexer;		// // //
	if (auto param = GetParameters<Hex<2>>(mml_))
		return writeState(&Track::q, requires(param.get<0>(), 0x01u, 0x7Fu, CMD_ILLEGAL("quantization", "q")));
	throw AMKd::Utility::SyntaxException {CMD_ERROR("quantization", "q")};		// // //
}

void Music::parsePanCommand() {
	using namespace AMKd::MML::Lexer;
	if (auto param = GetParameters<Int>(mml_)) {
		unsigned pan = requires(param.get<0>(), 0u, 20u, CMD_ILLEGAL("pan", "y"));
		bool sLeft = false, sRight = false;
		if (auto surround = GetParameters<Comma, Bool, Comma, Bool>(mml_))
			std::tie(sLeft, sRight) = *surround;
		return doPan(pan, sLeft, sRight);
	}

	throw AMKd::Utility::SyntaxException {CMD_ERROR("pan", "y")};		// // //
}

void Music::parseTempoCommand() {
	using namespace AMKd::MML::Lexer;		// // //
	if (auto param = GetParameters<Int>(mml_))
		return doTempo(requires(param.get<0>(), 0x00u, 0xFFu, CMD_ILLEGAL("tempo", "t")));
	throw AMKd::Utility::SyntaxException {CMD_ERROR("tempo", "t")};
}

void Music::parseTransposeDirective() {
	using namespace AMKd::MML::Lexer;		// // //
	if (auto param = GetParameters<Sep<'['>, Int, Sep<']'>, Sep<'='>, SInt>(mml_)) {
		unsigned inst; int trsp;
		std::tie(inst, trsp) = *param;
		while (true) {
			transposeMap[requires(inst++, 0u, 0xFFu, DIR_ILLEGAL("tuning"))] = trsp;
			auto ext = GetParameters<Comma, SInt>(mml_);
			if (!ext)
				return;
			std::tie(trsp) = *ext;
		}
	}

	throw AMKd::Utility::SyntaxException {DIR_ERROR("tuning")};
}

void Music::parseOctaveDirective() {
	using namespace AMKd::MML::Lexer;		// // //
	if (auto param = GetParameters<Int>(mml_))
		return doOctave(param.get<0>());
	throw AMKd::Utility::SyntaxException {DIR_ERROR("octave (\"o\")")};
}

void Music::parseInstrumentCommand() {
	using namespace AMKd::MML::Lexer;
	if (auto param = GetParameters<Int>(mml_)) {
		int inst = requires(param.get<0>(), 0x00u, 0xFFu, CMD_ILLEGAL("instrument", "@"));
		return doInstrument(inst, inst < 0x13 || inst >= 30);
	}
	throw AMKd::Utility::SyntaxException {CMD_ERROR("instrument", "@")};		// // //
}

// // //
void Music::parseDirectInstCommand() {
	using namespace AMKd::MML::Lexer;
	if (auto param = GetParameters<Int>(mml_))
		return doInstrument(requires(param.get<0>(), 0x00u, 0xFFu, CMD_ILLEGAL("instrument", "@")), true);
	throw AMKd::Utility::SyntaxException {CMD_ERROR("instrument", "@")};		// // //
}

void Music::parseOpenParenCommand() {
	using namespace AMKd::MML::Lexer;		// // //
	if (auto param = GetParameters<Int>(mml_)) {
		int label = requires(param.get<0>(), 0u, 0xFFFFu, "Illegal value for loop label.");
		if (GetParameters<Sep<')', '['>>(mml_)) {		// If this is a loop definition...
			loopLabel = label + 1;
			getActiveTrack().isDefiningLabelLoop = true;		// The rest of the code is handled in the respective function.
			doLoopLabel(label);		// // //
			return doLoopEnter();
		}
		if (auto loop = GetParameters<Sep<')'>, Option<Int>>(mml_)) {		// Otherwise, if this is a loop call...
			auto it = loopPointers.find(label + 1);
			if (it == loopPointers.cend())
				throw AMKd::Utility::ParamException {"Label not yet defined."};
			loopLabel = label + 1;
			doLoopRemoteCall(requires(loop->value_or(1), 0x01u, 0xFFu, "Invalid loop count."), it->second);		// // //
			loopLabel = 0;
			return;
		}
		throw AMKd::Utility::SyntaxException {"Error parsing label loop."};
	}
	
	int sampID;
	if (auto param = GetParameters<Sep<'@'>, Int>(mml_))
		sampID = instrToSample[requires(param.get<0>(), 0u, 29u, "Illegal instrument number for sample load command.")];
	else if (auto param2 = GetParameters<QString>(mml_)) {
		auto it = std::find(mySamples.cbegin(), mySamples.cend(), getSample(basepath / param2.get<0>(), name));		// // //
		if (it == mySamples.cend())
			throw AMKd::Utility::ParamException {"The specified sample was not included in this song."};
		sampID = std::distance(mySamples.cbegin(), it);
	}
	else
		throw AMKd::Utility::SyntaxException {"Error parsing sample load command."};

	if (auto ext = GetParameters<Comma, Byte, Sep<')'>>(mml_))
		return doSampleLoad(sampID, ext.get<0>());
	throw AMKd::Utility::SyntaxException {"Error parsing sample load command."};
}

// // //
void Music::parseRemoteCodeCommand() {
	using namespace AMKd::MML::Lexer;

	if (targetAMKVersion < 2)
		throw AMKd::Utility::SyntaxException {"Remote code commands requires #amk 2 or above."};

	if (auto param = GetParameters<Int, Comma, SInt>(mml_)) {		// // // A channel's been defined, we're parsing a remote
		if (!channelDefined)
			throw AMKd::Utility::MMLException {"TODO: allow calling remote codes inside definitions"};

		int remoteID = param.get<0>();
		int remoteOpt = param.get<1>();
		int remoteLen = 0;
		if (remoteOpt == AMKd::Binary::CmdOptionFC::Sustain || remoteOpt == AMKd::Binary::CmdOptionFC::Release) {
			if (auto len = GetParameters<Comma, Byte>(mml_))
				remoteLen = len.get<0>();
			else if (auto len2 = GetParameters<Comma, Dur>(mml_)) {		// // //
				remoteLen = getRawTicks(len2.get<0>());
				if (remoteLen > 0x100)
					throw AMKd::Utility::ParamException {"Note length specified was too large."};		// // //
				else if (remoteLen == 0x100)
					remoteLen = 0;
			}
			else
				throw AMKd::Utility::SyntaxException {"Error parsing remote code setup."};
		}

		if (!GetParameters<Sep<')'>, Sep<'['>>(mml_))
			throw AMKd::Utility::SyntaxException {"Error parsing remote code setup."};

		if (loopPointers.find(remoteID) == loopPointers.cend())		// // //
			loopPointers.insert({remoteID, static_cast<uint16_t>(-1)});
		getActiveTrack().AddLoopPosition(1);
		return doRemoteCallNative(loopPointers[remoteID], remoteOpt, remoteLen);		// // //
	}

	if (auto param = GetParameters<Int, Sep<')'>, Sep<'['>>(mml_)) {		// // // We're outside of a channel, this is a remote call definition.
		if (channelDefined)
			throw AMKd::Utility::MMLException {"TODO: allow inline remote code definitions"};
		loopLabel = param.get<0>();
		inRemoteDefinition = true;
		if (loopLabel > 0) // TODO: check
			doLoopLabel(loopLabel - 1);
		return doLoopEnter();
	}

	throw AMKd::Utility::SyntaxException {"Error parsing remote code command."};
}

void Music::parseLoopEndCommand() {
	using namespace AMKd::MML::Lexer;		// // //
	return doLoopExit(requires(GetParameters<Option<Int>>(mml_)->value_or(1), 0x01u, 0xFFu, "Invalid loop count."));
}

// // //
void Music::parseSubloopEndCommand() {
	using namespace AMKd::MML::Lexer;		// // //
	if (auto param = GetParameters<Int>(mml_))
		return doSubloopExit(requires(param.get<0>(), 2u, 256u, CMD_ILLEGAL("subloop", "[[ ]]")));
	throw AMKd::Utility::SyntaxException {CMD_ERROR("subloop", "[[ ]]")};
}

void Music::parseStarLoopCommand() {	
	using namespace AMKd::MML::Lexer;		// // //
	return doLoopRemoteCall(requires(GetParameters<Option<Int>>(mml_)->value_or(1),
									 0x01u, 0xFFu, CMD_ILLEGAL("star loop", "*")), prevLoop);
}

void Music::parseVibratoCommand() {
	using namespace AMKd::MML::Lexer;		// // //
	if (auto param = GetParameters<Int, Comma, Int>(mml_)) {
		unsigned delay, rate, depth;
		if (auto param2 = GetParameters<Comma, Int>(mml_)) {
			std::tie(delay, rate) = *param;
			std::tie(depth) = *param2;
		}
		else {
			delay = 0;
			std::tie(rate, depth) = *param;
		}
		return doVibrato(requires(delay, 0x00u, 0xFFu, "Illegal value for vibrato delay."),
						 requires(rate, 0x00u, 0xFFu, "Illegal value for vibrato rate."),
						 requires(depth, 0x00u, 0xFFu, "Illegal value for vibrato extent."));		// // //
	}

	throw AMKd::Utility::SyntaxException {CMD_ERROR("vibrato", "p")};		// // //
}

void Music::parseHFDInstrumentHack(int addr, int bytes) {
	using namespace AMKd::MML::Lexer;		// // //

	int byteNum = 0;
	for (++bytes; bytes > 0; --bytes) {
		if (auto param = GetParameters<Byte>(mml_)) {
			instrumentData.push_back(param.get<0>());		// // //
			++addr;
			++byteNum;
			if (byteNum == 1)
				usedSamples[param.get<0>()] = true;
			else if (byteNum == 5) {
				instrumentData.push_back(0);	// On the 5th byte, we need to add a 0 as the new sub-multiplier.
				byteNum = 0;
			}
			continue;
		}
		throw AMKd::Utility::SyntaxException {"Unknown HFD hex command."};
	}
}

void Music::parseHFDHex() {
	using namespace AMKd::MML::Lexer;		// // //
	auto kind = GetParameters<Byte>(mml_);
	if (!kind)
		throw AMKd::Utility::SyntaxException {"Unknown HFD hex command."};

	if (convert) {
		switch (kind.get<0>()) {
		case 0x80:
			if (auto param = GetParameters<Byte, Byte>(mml_)) {		// // //
				unsigned reg, val;
				std::tie(reg, val) = *param;
				return (reg == 0x6D || reg == 0x7D) ? (void)(songTargetProgram = Target::AM4) : // Do not write the HFD header hex bytes.
					reg == 0x6C ? doNoise(val) : // Noise command gets special treatment.
					doDSPWrite(reg, val);		// // //
			}
			throw AMKd::Utility::SyntaxException {"Error while parsing HFD hex command."};
		case 0x81:
			if (auto param = GetParameters<Byte, Byte>(mml_))		// // //
				return doTranspose(param.get<0>());
			throw AMKd::Utility::SyntaxException {"Error while parsing HFD hex command."};
		case 0x82:
			if (auto param = GetParameters<Byte, Byte, Byte, Byte>(mml_)) {		// // //
				int addr = (param.get<0>() << 8) | param.get<1>();
				int bytes = (param.get<2>() << 8) | param.get<3>();
				if (addr == 0x6136)
					return parseHFDInstrumentHack(addr, bytes);
				while (bytes-- >= 0)		// // //
					if (auto val = GetParameters<Byte>(mml_))
						doARAMWrite(addr++, val.get<0>());
					else
						throw AMKd::Utility::SyntaxException {"Error while parsing HFD hex command."};
				return;
			}
			throw AMKd::Utility::SyntaxException {"Error while parsing HFD hex command."};
		default:
			if (kind.get<0>() >= 0x80)		// // //
				throw AMKd::Utility::ParamException {"Unknown HFD hex command type."};
		}
	}

	if (auto param = GetParameters<Byte>(mml_))		// // //
		return doEnvelope(kind.get<0>(), param.get<0>());
	throw AMKd::Utility::ParamException {"Unknown hex command type."};
}

// // //
void Music::insertRemoteConversion(uint8_t cmdtype, uint8_t param, const AMKd::Binary::Stream &cmd) {
	getActiveTrack().AddRemoteConversion(cmdtype, param, cmd);
}

void Music::insertRemoteConversion(uint8_t cmdtype, uint8_t param, AMKd::Binary::Stream &&cmd) {
	getActiveTrack().AddRemoteConversion(cmdtype, param, std::move(cmd));
}

// // //

void Music::parseHexCommand() {
	using namespace AMKd::MML::Lexer;

	mml_.Unput();
	const uint8_t currentHex = [&] {
		auto param = GetParameters<Byte>(mml_);
		return param ? param.get<0>() : throw AMKd::Utility::SyntaxException {"Error parsing hex command."};
	}();

	if (!validateHex)
		return doDirectWrite(currentHex);

	switch (currentHex) {		// // //
	case AMKd::Binary::CmdType::Inst:
		if (auto param = GetParameters<Byte>(mml_))
			return doInstrument(param.get<0>(), true);
		break;
	case AMKd::Binary::CmdType::Pan:
		if (auto param = GetParameters<Byte>(mml_))
			return doPan(param.get<0>() & 0x3F, param.get<0>() & 0x80, param.get<0>() & 0x40);
		break;
	case AMKd::Binary::CmdType::PanFade:
		if (auto param = GetParameters<Byte, Byte>(mml_))
			return doPanFade(param.get<1>(), param.get<0>());
		break;
	case AMKd::Binary::CmdType::Portamento:
		if (auto param = GetParameters<Byte, Byte, Byte>(mml_))
			return doPortamento(param.get<0>(), param.get<1>(), param.get<2>());
		break;
	case AMKd::Binary::CmdType::Vibrato:
		if (auto param = GetParameters<Byte, Byte, Byte>(mml_))
			return doVibrato(param.get<0>(), param.get<1>(), param.get<2>());
		break;
	case AMKd::Binary::CmdType::VibratoOff:
		return doVibratoOff();
	case AMKd::Binary::CmdType::VolGlobal:
		if (auto param = GetParameters<Byte>(mml_))
			return doGlobalVolume(param.get<0>());
		break;
	case AMKd::Binary::CmdType::VolGlobalFade:
		if (auto param = GetParameters<Byte, Byte>(mml_))
			return doGlobalVolume(param.get<1>(), param.get<0>());
		break;
	case AMKd::Binary::CmdType::Tempo:
		if (auto param = GetParameters<Byte>(mml_))
			return doTempo(param.get<0>());
		break;
	case AMKd::Binary::CmdType::TempoFade:
		if (auto param = GetParameters<Byte, Byte>(mml_))
			return doTempo(param.get<1>(), param.get<0>());
		break;
	case AMKd::Binary::CmdType::TrspGlobal:
		if (auto param = GetParameters<Byte>(mml_))
			return doTransposeGlobal(param.get<0>());
		break;
	case AMKd::Binary::CmdType::Tremolo:
		if (auto first = GetParameters<Byte>(mml_)) {		// // //
			if (songTargetProgram == Target::AM4 && first.get<0>() >= 0x80) {
				auto samp = first.get<0>() & 0x7F;
				if (mySamples.empty() && samp > 0x13)
					throw AMKd::Utility::ParamException {"This song uses custom samples, but has not yet defined its samples with the #samples command."};		// // //
				if (auto param = GetParameters<Byte>(mml_))
					return doSampleLoad(samp, param.get<0>());
			}
			else if (auto remain = GetParameters<Byte, Byte>(mml_))
				return doTremolo(first.get<0>(), remain.get<0>(), remain.get<1>());
		}
		break;
	case AMKd::Binary::CmdType::Subloop:
		if (songTargetProgram == Target::AM4)		// // // N-SPC tremolo off
			return doTremoloOff();		// // //
		if (auto param = GetParameters<Byte>(mml_)) {
			int repeats = param.get<0>();
			return repeats ? doSubloopExit(repeats + 1) : doSubloopEnter();
		}
		break;
	case AMKd::Binary::CmdType::Vol:
		if (auto param = GetParameters<Byte>(mml_))
			return doVolume(param.get<0>());
		break;
	case AMKd::Binary::CmdType::VolFade:
		if (auto param = GetParameters<Byte, Byte>(mml_))
			return doVolume(param.get<1>(), param.get<0>());
		break;
	case AMKd::Binary::CmdType::Loop:
		if (auto param = GetParameters<Byte, Byte, Byte>(mml_))
			return doLoopNative(param.get<0>() | (param.get<1>() << 8), param.get<2>());		// // //
		break;
	case AMKd::Binary::CmdType::VibratoFade:
		if (auto param = GetParameters<Byte>(mml_))
			return doVibratoFade(param.get<0>());
		break;
	case AMKd::Binary::CmdType::BendAway:
		if (auto param = GetParameters<Byte, Byte, Byte>(mml_))
			return doPitchBend(param.get<0>(), param.get<1>(), param.get<2>(), true);
		break;
	case AMKd::Binary::CmdType::BendTo:
		if (auto param = GetParameters<Byte, Byte, Byte>(mml_))
			return doPitchBend(param.get<0>(), param.get<1>(), param.get<2>(), false);
		break;
	case AMKd::Binary::CmdType::Envelope:
		if (songTargetProgram == Target::AM4)
			return parseHFDHex();
		if (auto param = GetParameters<Byte, Byte>(mml_))
			return doEnvelope(param.get<0>(), param.get<1>());
		break;
	case AMKd::Binary::CmdType::Detune:
		if (auto param = GetParameters<Byte>(mml_))
			return doDetune(param.get<0>());
		break;
	case AMKd::Binary::CmdType::EchoVol:
		if (auto param = GetParameters<Byte, Byte, Byte>(mml_))
			return doEchoVolume(param.get<2>(), param.get<0>(), param.get<1>());
		break;
	case AMKd::Binary::CmdType::EchoOff:
		return doEchoOff();
	case AMKd::Binary::CmdType::EchoParams:
		if (auto param = GetParameters<Byte, Byte, Byte>(mml_))
			return doEchoParams(param.get<0>(), param.get<1>(), param.get<2>());
		break;
	case AMKd::Binary::CmdType::EchoFade:
		if (auto param = GetParameters<Byte, Byte, Byte>(mml_))
			return doEchoFade(param.get<1>(), param.get<2>(), param.get<0>());
		break;
	case AMKd::Binary::CmdType::SampleLoad:
		if (auto param = GetParameters<Byte, Byte>(mml_))
			return doSampleLoad(param.get<0>(), param.get<1>());
		break;
	case AMKd::Binary::CmdType::ExtF4:
		if (auto param = GetParameters<Byte>(mml_)) {
			auto cmdType = param.get<0>();
			switch (cmdType) {
			case AMKd::Binary::CmdOptionF4::YoshiCh5:      return doYoshiDrums(true);
			case AMKd::Binary::CmdOptionF4::Legato:        return doLegato();
			case AMKd::Binary::CmdOptionF4::LightStaccato: return doLightStaccato();
			case AMKd::Binary::CmdOptionF4::EchoToggle:    return doEchoToggle();
			case AMKd::Binary::CmdOptionF4::Sync:          return doSync();
			case AMKd::Binary::CmdOptionF4::Yoshi:         return doYoshiDrums(false);
			case AMKd::Binary::CmdOptionF4::TempoImmunity: return doTempoImmunity();
			case AMKd::Binary::CmdOptionF4::VelocityTable: return doVolumeTable(true);
			case AMKd::Binary::CmdOptionF4::RestoreInst:   return doRestoreInst();
			}
			throw AMKd::Utility::ParamException {"Unknown $F4 hex command type."};
		}
		break;
	case AMKd::Binary::CmdType::FIR:
		if (auto param = GetParameters<Byte, Byte, Byte, Byte, Byte, Byte, Byte, Byte>(mml_))
			return std::apply([this] (auto... args) { return doFIRFilter(args...); }, *param);
		break;
	case AMKd::Binary::CmdType::DSP:
		if (auto param = GetParameters<Byte, Byte>(mml_))
			return doDSPWrite(param.get<0>(), param.get<1>());
		break;
	case AMKd::Binary::CmdType::ARAM:
		if (auto param = GetParameters<Byte, Byte, Byte>(mml_))
			return doARAMWrite((param.get<1>() << 8) | param.get<2>(), param.get<0>());
		break;
	case AMKd::Binary::CmdType::Noise:
		if (auto param = GetParameters<Byte>(mml_))
			return doNoise(param.get<0>());
		break;
	case AMKd::Binary::CmdType::DataSend:
		if (auto param = GetParameters<Byte, Byte>(mml_))
			return doDataSend(param.get<0>(), param.get<1>());
		break;
	case AMKd::Binary::CmdType::ExtFA:
		if (auto param = GetParameters<Byte, Byte>(mml_)) {
			using namespace AMKd::Binary;
			uint8_t cmdType, cmdVal;
			std::tie(cmdType, cmdVal) = *param;
			switch (cmdType) {
			case CmdOptionFA::PitchMod:   return doPitchMod(cmdVal);
			case CmdOptionFA::Gain:       return doGain(cmdVal);
			case CmdOptionFA::Transpose:  return doTranspose(cmdVal);
			case CmdOptionFA::Amplify:    return doAmplify(cmdVal);
			case CmdOptionFA::EchoBuffer: return doEchoBuffer(cmdVal);
			case CmdOptionFA::GainOld:
				if (targetAMKVersion < 2) {		// // //
					//if (tempoRatio != 1)
					//	throw AMKd::Utility::ParamException {"#halvetempo cannot be used on AMK 1 songs that use the $FA $05 or old $FC command."};
					auto &track = getBaseTrack();		// // //
					track.usingFA = (cmdVal != 0);		// // //
					if (track.usingFA) {
						auto gain = Stream() << ChunkAMK::Gain(cmdVal) << ChunkNSPC::End();
						if (track.usingFC)
							insertRemoteConversion(CmdOptionFC::KeyOffConv, track.lastFCDelayValue, gain);
						else {
							insertRemoteConversion(CmdOptionFC::KeyOn, 0x00, Stream() << ChunkAMK::RestoreInst() << ChunkNSPC::End());
							insertRemoteConversion(CmdOptionFC::KeyOff, 0x00, gain);
						}
					}
					else
						insertRemoteConversion(CmdOptionFC::Disable, 0x00, Stream());
					return;
				}
				throw AMKd::Utility::ParamException {"$FA $05 in #amk 2 or above has been replaced with remote code."};
			case CmdOptionFA::VolTable:
				return doVolumeTable(requires(static_cast<unsigned>(cmdVal), 0u, 1u, "Invalid volume table index."));
			}
			throw AMKd::Utility::ParamException {"Unknown $FA hex command type."};
		}
		break;
	case AMKd::Binary::CmdType::Arpeggio:
		if (auto param = GetParameters<Byte, Byte>(mml_)) {
			unsigned count, len;
			std::tie(count, len) = *param;
			switch (count) {
			case AMKd::Binary::ArpOption::Trill:
				if (auto offset = GetParameters<Byte>(mml_))
					return doTrill(len, offset.get<0>());
				break;
			case AMKd::Binary::ArpOption::Glissando:
				if (auto offset = GetParameters<Byte>(mml_))
					return doGlissando(len, offset.get<0>());
				break;
			default:
				if (count < 0x80) {
					std::vector<uint8_t> notes;
					for (unsigned j = 0; j < count; ++j) {
						auto note = GetParameters<Byte>(mml_);
						note ? notes.push_back(note.get<0>()) :
							throw AMKd::Utility::SyntaxException {"Incorrect number of notes for arpeggio command."};
					}
					return doArpeggio(len, notes);
				}
			}
			throw AMKd::Utility::ParamException {"Illegal value for arpeggio command."};
		}
		break;
	case AMKd::Binary::CmdType::Callback:
		if (targetAMKVersion < 2) {		// // //
			if (auto param = GetParameters<Byte, Byte>(mml_)) {
				using namespace AMKd::Binary;
				//if (tempoRatio != 1) error("#halvetempo cannot be used on AMK 1 songs that use the $FA $05 or old $FC command.")
				auto &track = getBaseTrack();		// // //
				uint8_t delay, gain;
				std::tie(delay, gain) = *param;

				auto s = Stream() << ChunkAMK::Gain(gain) << ChunkNSPC::End();
				track.usingFC = (delay != 0);
				if (track.usingFC) {
					track.lastFCDelayValue = static_cast<uint8_t>(divideByTempoRatio(delay, false));		// // //
					if (track.usingFA)
						insertRemoteConversion(CmdOptionFC::KeyOffConv, track.lastFCDelayValue, s);
					else {
						insertRemoteConversion(CmdOptionFC::KeyOn, 0x00, Stream() << ChunkAMK::RestoreInst() << ChunkNSPC::End());
						insertRemoteConversion(CmdOptionFC::Release, track.lastFCDelayValue, s);
					}
				}
				else {
					if (track.usingFA)
						insertRemoteConversion(CmdOptionFC::KeyOff, 0x00, s); // check this
					else
						insertRemoteConversion(CmdOptionFC::Disable, 0x00, Stream());
				}
				return;
			}
			break;
		}

		if (auto param = GetParameters<Byte, Byte, Byte, Byte>(mml_))
			return doRemoteCallNative(param.get<0>() | (param.get<1>() << 8), param.get<2>(), param.get<3>());		// // //
		break;
	case 0xFD: case 0xFE: case 0xFF:
		break;
	default:
		if (targetAMKVersion == 0 && currentHex < AMKd::Binary::CmdType::Inst) {
			if (std::exchange(manualNoteWarning, false))
				warn("Warning: A hex command was found that will act as a note instead of a special\n"
					 "effect. If this is a song you're using from someone else, you can most likely\n"
					 "ignore this message, though it may indicate that a necessary #amm or #am4 is\n"
					 "missing.");		// // //
			return;
		}
	}

	throw AMKd::Utility::ParamException {"Unknown hex command type."};
}

void Music::parseNote(int note) {		// // //
	using namespace AMKd::MML::Lexer;		// // //
	AMKd::MML::Duration dur = note == AMKd::Binary::CmdType::Rest ?
		GetParameters<RestDur>(mml_).get<0>() : GetParameters<Dur>(mml_).get<0>();

	bool hasPortamento = GetParameters<Sep<'&'>>(mml_).success();
	int bendTicks = hasPortamento ? getLastTicks(dur) : 0;
	if (!hasPortamento && mml_.Trim("\\$DD", true)) {
		mml_.Unput();
		bendTicks = getLastTicks(dur);
	}

	doNote(note, getFullTicks(dur), bendTicks, hasPortamento);
}

// // //
void Music::parseNoteCommon(int offset) {
	//am4silence++;
	using namespace AMKd::MML::Lexer;		// // //
	int note = getPitch(offset + GetParameters<Acc>(mml_)->offset);
	if (getActiveTrack().instrument >= 21 && getActiveTrack().instrument < 30) {		// // //
		note = 0xD0 + (getActiveTrack().instrument - 21);
		if (!(channel == 6 || channel == 7 || (inNormalLoop && (prevChannel == 6 || prevChannel == 7))))	// If this is not a SFX channel,
			getActiveTrack().instrument = 0xFF;										// Then don't force the drum pitch on every note.
	}
	return parseNote(note);
}

void Music::parseNoteC() {
	return parseNoteCommon(0);
}

void Music::parseNoteD() {
	return parseNoteCommon(2);
}

void Music::parseNoteE() {
	return parseNoteCommon(4);
}

void Music::parseNoteF() {
	return parseNoteCommon(5);
}

void Music::parseNoteG() {
	return parseNoteCommon(7);
}

void Music::parseNoteA() {
	return parseNoteCommon(9);
}

void Music::parseNoteB() {
	return parseNoteCommon(11);
}

void Music::parseTie() {
	return parseNote(AMKd::Binary::CmdType::Tie);
}

void Music::parseRest() {
	return parseNote(AMKd::Binary::CmdType::Rest);
}

void Music::parseHDirective() {
	using namespace AMKd::MML::Lexer;		// // //
	if (auto param = GetParameters<SInt>(mml_)) {
		getActiveTrack().usingH = true;		// // //
		getActiveTrack().h = static_cast<int8_t>(
			requires(param.get<0>(), -128, 127, DIR_ILLEGAL("transpose (\"h\")")));
		return;
	}
	throw AMKd::Utility::SyntaxException {DIR_ERROR("transpose (\"h\")")};
}

void Music::parseNCommand() {
	using namespace AMKd::MML::Lexer;		// // //
	if (auto param = GetParameters<Hex<2>>(mml_))
		return doNoise(requires(param.get<0>(), 0x00u, 0x1Fu, CMD_ILLEGAL("noise", "n")));		// // //
	throw AMKd::Utility::SyntaxException {CMD_ERROR("noise", "n")};
}

void Music::parseOptionDirective() {
	if (targetAMKVersion < 2)		// // //
		throw AMKd::Utility::SyntaxException {"#option is only supported on #amk 2 or above."};
	if (channelDefined)
		throw AMKd::Utility::SyntaxException {"#option directives must be used before any and all channels."};		// // //

	static const AMKd::Utility::Trie<void (Music::*)()> CMDS {		// // //
		{"smwvtable", &Music::parseSMWVTable},
		{"nspcvtable", &Music::parseNSPCVTable},
		{"tempoimmunity", &Music::parseTempoImmunity},
		{"noloop", &Music::parseNoLoop},
		{"dividetempo", &Music::parseDivideTempo},
	};

	using namespace AMKd::MML::Lexer;		// // //
	if (auto param = GetParameters<String>(mml_)) {
		downcase(param.get<0>());
		std::string_view sv {param.get<0>()};
		if (auto func = CMDS.SearchValue(sv))
			return (this->*(*func))();
	}
	throw AMKd::Utility::ParamException {"Unknown option type for '#option' directive."};		// // //
}

// // //
void Music::parseSMWVTable() {
	if (!std::exchange(usingSMWVTable, true))
		doVolumeTable(false);
	else
		warn("This song is already using the SMW volume table. This command is just wasting three bytes...");		// // //
}

void Music::parseNSPCVTable() {
	if (std::exchange(usingSMWVTable, false))
		doVolumeTable(true);
	else
		warn("This song is already using the N-SPC volume table. This command is just wasting two bytes...");		// // //
}

void Music::parseTempoImmunity() {
	return doTempoImmunity();		// // //
}

void Music::parseNoLoop() {
	doesntLoop = true;
}

void Music::parseDivideTempo() {
	using namespace AMKd::MML::Lexer;
	if (auto param = GetParameters<Int>(mml_)) {
		if (int divide = param.get<0>()) {
			tempoRatio = divide;
			return;
		}
		throw AMKd::Utility::ParamException {"Argument for #option dividetempo cannot be 0."};		// // //
	}
	throw AMKd::Utility::SyntaxException {"Missing integer argument for #option dividetempo."};		// // //
}

void Music::parseHalveTempo() {
	if (channelDefined)
		throw AMKd::Utility::SyntaxException {"#halvetempo must be used before any and all channels."};		// // //
	tempoRatio = requires(tempoRatio * 2, 1, 256, "#halvetempo has been used too many times...what are you even doing?");
}

void Music::parseSpecialDirective() {
	static const AMKd::Utility::Trie<void (Music::*)()> CMDS {		// // //
		{"instruments", &Music::parseInstrumentDefinitions},
		{"samples", &Music::parseSampleDefinitions},
		{"pad", &Music::parsePadDefinition},
		{"spc", &Music::parseSPCInfo},
		{"louder", &Music::parseLouderCommand},
		{"tempoimmunity", &Music::parseTempoImmunity},
		{"path", &Music::parsePath},
		{"halvetempo", &Music::parseHalveTempo},
		{"option", &Music::parseOptionDirective},
	};

	using namespace AMKd::MML::Lexer;		// // //
	if (auto param = GetParameters<String>(mml_)) {
		downcase(param.get<0>());
		std::string_view sv {param.get<0>()};
		if (auto func = CMDS.SearchValue(sv))
			return (this->*(*func))();
	}
	throw AMKd::Utility::SyntaxException {DIR_ERROR("'#'")};
}

void Music::parseInstrumentDefinitions() {
	using namespace AMKd::MML::Lexer;		// // //

	const auto pushInst = [this] (int inst) {
		auto param = GetParameters<Multi<Byte>>(mml_);
		if (!param || param->size() != 5)
			throw AMKd::Utility::SyntaxException {"Error parsing instrument definition; there must be 5 bytes following the sample."};
		instrumentData.push_back(static_cast<uint8_t>(inst));		// // //
		for (const auto &x : param.get<0>())
			instrumentData.push_back(std::get<0>(x));
		usedSamples[inst] = true;
	};
	
	if (GetParameters<Sep<'{'>>(mml_)) {
		while (true) {
			if (auto brr = GetParameters<QString>(mml_)) {		// // //
				const std::string &brrName = brr.get<0>();
				if (brrName.empty())
					throw AMKd::Utility::SyntaxException {"Error parsing sample portion of the instrument definition."};
				auto it = std::find(mySamples.cbegin(), mySamples.cend(), getSample(basepath / brrName, name));		// // //
				if (it == mySamples.cend())
					throw AMKd::Utility::ParamException {"The specified sample was not included in this song."};		// // //
				pushInst(std::distance(mySamples.cbegin(), it));
			}
			else if (auto noi = GetParameters<Sep<'n'>, Hex<2>>(mml_))
				pushInst(0x80 | requires(noi.get<0>(), 0x00u, 0x1Fu,
										 "Invalid noise pitch.  The value must be a hex value from 0 - 1F."));
			else if (auto inst = GetParameters<Sep<'@'>, Int>(mml_))
				pushInst(instrToSample[requires(inst.get<0>(), 0u, 29u,
												"Cannot use a custom instrument's sample as a base for another custom instrument.")]);
			else if (GetParameters<Sep<'}'>>(mml_))
				return;
			else
				throw AMKd::Utility::SyntaxException {"Unexpected end of instrument definition."};
		}
	}
	throw AMKd::Utility::SyntaxException {"Could not find opening curly brace in instrument definition."};
}

void Music::parseSampleDefinitions() {
	using namespace AMKd::MML::Lexer;		// // //

	if (GetParameters<Sep<'{'>>(mml_)) {
		while (true) {		// // //
			if (auto param = GetParameters<QString>(mml_)) {
				fs::path tempstr = basepath / param.get<0>();		// // //
				auto extension = tempstr.extension();
				if (extension == ".bnk")
					addSampleBank(tempstr, this);
				else if (extension == ".brr")
					addSample(tempstr, this, false);
				else
					throw AMKd::Utility::ParamException {"The filename for the sample was invalid.  Only \".brr\" and \".bnk\" are allowed."};		// // //
			}
			else if (auto group = GetParameters<Sep<'#'>, Ident>(mml_))
				addSampleGroup(group.get<0>(), this);		// // //
			else
				break;
		}
		if (GetParameters<Sep<'}'>>(mml_))
			return;
		throw AMKd::Utility::SyntaxException {"Unexpected end of sample group definition."};
	}
	throw AMKd::Utility::SyntaxException {"Could not find opening brace in sample group definition."};
}

void Music::parsePadDefinition() {
	using namespace AMKd::MML::Lexer;
	if (GetParameters<Sep<'$'>>(mml_)) {		// // //
		if (auto param = HexInt()(mml_)) {
			minSize = *param;
			return;
		}
	}

	throw AMKd::Utility::SyntaxException {DIR_ERROR("padding")};		// // //
}

void Music::parseLouderCommand() {
	if (targetAMKVersion > 1)
		warn("#louder is redundant in #amk 2 and above.");		// // //
	doVolumeTable(true);		// // //
}

void Music::parsePath() {
	using namespace AMKd::MML::Lexer;		// // //
	if (auto param = GetParameters<QString>(mml_))
		basepath = fs::path {"."} / param.get<0>();
	else
		throw AMKd::Utility::SyntaxException {"Unexpected symbol found in path command. Expected a quoted string."};
}

// // //
Track &Music::getActiveTrack() {
	return ::channel == CHANNELS ? loopTrack : tracks[::channel];
}

const Track &Music::getActiveTrack() const {
	return ::channel == CHANNELS ? loopTrack : tracks[::channel];
}

Track &Music::getBaseTrack() {
	return tracks[inNormalLoop ? prevChannel : channel];
}

int Music::getPitch(int i) {
	i += (getActiveTrack().o.Get() - 1) * 12 + 0x80;		// // //
	if (getActiveTrack().usingH)		// // //
		i += getActiveTrack().h;
	else if (!getActiveTrack().ignoreTuning)		// // // More AM4 tuning stuff
		i -= transposeMap[getActiveTrack().instrument];
	return requires(i, 0x80, static_cast<int>(AMKd::Binary::CmdType::Tie) - 1, "Note's pitch is out of range.");
}

// // //
int Music::getRawTicks(const AMKd::MML::Duration &dur) const {
	return checkTickFraction(dur.GetTicks() / tempoRatio);
}

int Music::getFullTicks(const AMKd::MML::Duration &dur) const {
	return checkTickFraction(dur.GetTicks(getActiveTrack().l.Get()) / tempoRatio * (getActiveTrack().inTriplet ? 2. / 3. : 1.));
}

int Music::getLastTicks(const AMKd::MML::Duration &dur) const {
	return checkTickFraction(dur.GetLastTicks(getActiveTrack().l.Get()) / tempoRatio * (getActiveTrack().inTriplet ? 2. / 3. : 1.));
}

int Music::checkTickFraction(double ticks) const {
	auto ret = static_cast<int>(ticks + .5);
	if (/*fractionIsError*/ false) {
		auto v1 = static_cast<intmax_t>(ticks / AMKd::MML::Duration::EPSILON + .5);
		auto v2 = static_cast<intmax_t>(ret / AMKd::MML::Duration::EPSILON + .5);
		if (v1 != v2)
			warn("This note duration would produce a fractional tick count.");
	}
	return ret;
}

// // //

void Music::pointersFirstPass() {
	if (std::all_of(std::cbegin(tracks), std::cend(tracks), [] (const Track &t) { return !t.HasData(); }))		// // //
		throw AMKd::Utility::InsertException {"This song contained no musical data!"};

	if (targetAMKVersion == 1) {			// Handle more conversion of the old $FC command to remote call.
		for (Track &t : tracks)
			t.InsertRemoteCalls(loopTrack);		// // //
		loopTrack.InsertRemoteCalls(loopTrack);
	}

	for (Track &t : tracks)		// // //
		if (t.HasData())
			t.Append(AMKd::Binary::ChunkNSPC::End());

	if (mySamples.empty())		// // // If no sample groups were provided...
		addSampleGroup("default", this);		// // //

	if (optimizeSampleUsage) {
		int emptySampleIndex = ::getSample("EMPTY.brr", name);
		if (emptySampleIndex == -1) {
			addSample("EMPTY.brr", this, true);
			emptySampleIndex = getSample("EMPTY.brr", name);
		}

		for (size_t z = 0, n = mySamples.size(); z < n; ++z)		// // //
			if (!usedSamples[z] && !samples[mySamples[z]].important)
				mySamples[z] = static_cast<uint16_t>(emptySampleIndex);		// // //
	}

	const auto insertPtr = [this] (unsigned adr) {		// // //
		allPointersAndInstrs.push_back(static_cast<uint8_t>(adr));
		allPointersAndInstrs.push_back(static_cast<uint8_t>(adr >> 8));
	};

	int binpos = 0;		// // //
	for (Track &t : tracks) {
		if (t.HasData())
			t.phrasePointers[0] = static_cast<uint16_t>(binpos);
		t.phrasePointers[1] += t.phrasePointers[0];
		binpos += t.GetStreamSize();
	}

	headerSize = 20 + (hasIntro ? 18 : 0) + (doesntLoop ? 0 : 2) + instrumentData.size();		// // //
	instrumentPos = (hasIntro ? 4 : 2) + (doesntLoop ? 2 : 4);		// // //

	insertPtr(instrumentPos + instrumentData.size());		// // //
	if (hasIntro)
		insertPtr(instrumentPos + instrumentData.size() + 16);
	if (doesntLoop)
		insertPtr(0xFFFF); // 00 00
	else {
		insertPtr(0xFFFE); // FF 00
		insertPtr(hasIntro ? 0x0002 : 0x0000); // pointer to frame at loop point
	}
	allPointersAndInstrs.insert(allPointersAndInstrs.cend(), instrumentData.cbegin(), instrumentData.cend());
	for (Track &t : tracks)		// // //
		insertPtr(t.HasData() ? (t.phrasePointers[0] + headerSize) : 0xFFFF);
	if (hasIntro)
		for (Track &t : tracks)		// // //
			insertPtr(t.HasData() ? (t.phrasePointers[1] + headerSize) : 0xFFFF);

	totalSize = headerSize + loopTrack.GetStreamSize() + getDataSize();		// // //

	//if (tempo == -1) tempo = 0x36;
	mainLength = static_cast<unsigned>(-1);		// // //
	for (Track &t : tracks)
		if (t.channelLength != 0)		// // //
			mainLength = std::min(mainLength, (unsigned int)t.channelLength);
	if (static_cast<int>(mainLength) == -1)
		throw AMKd::Utility::InsertException {"This song doesn't seem to have any data."};		// // //

	unsigned totalLength = mainLength;		// // //
	if (hasIntro)
		mainLength -= introLength;

	if (guessLength) {
		double l1 = 0, l2 = 0;
		bool onL1 = true;

		std::sort(tempoChanges.begin(), tempoChanges.end());		// // //
		if (tempoChanges.empty() || tempoChanges[0].first != 0)
			tempoChanges.insert(tempoChanges.begin(), std::make_pair(0., 0x36)); // Stick the default tempo at the beginning if necessary.
		tempoChanges.emplace_back(totalLength, 0);		// // // Add in a dummy tempo change at the very end.

		for (size_t z = 0, n = tempoChanges.size() - 1; z < n; ++z) {		// // //
			if (tempoChanges[z].first > totalLength) {
				warn("A tempo change was found beyond the end of the song.");		// // //
				break;
			}

			if (tempoChanges[z].second < 0)
				onL1 = false;

			double difference = tempoChanges[z + 1].first - tempoChanges[z].first;
			(onL1 ? l1 : l2) += difference / (2 * std::abs(tempoChanges[z].second));
		}

		if (hasIntro) {
			seconds = static_cast<unsigned>(l1 + l2 * 2 + 0.5);		// // // Just 2? Not 2.012584 or something?  Wow.
			mainSeconds = l2;
			introSeconds = l1;
		}
		else {
			mainSeconds = l1;
			seconds = static_cast<unsigned>(l1 * 2 + 0.5);		// // //
		}
		knowsLength = true;
	}
}

void Music::displaySongData() const {
	int spaceUsedBySamples = 0;
	for (const uint16_t x : mySamples)		// // //
		spaceUsedBySamples += 4 + samples[x].data.size();	// The 4 accounts for the space used by the SRCN table.

	if (verbose) {
		std::cout << name << " total size: 0x" << hex4 << totalSize << std::dec << " bytes\n";		// // //
		const hex_formatter hex3 {3};
		std::cout << '\t';		// // //
		for (size_t i = 0; i < CHANNELS / 2; ++i)
			std::cout << "#" << i << ": 0x" << hex3 << tracks[i].GetStreamSize() << ' ';
		std::cout << "Ptrs+Instrs: 0x" << hex3 << headerSize << "\n\t";
		for (size_t i = CHANNELS / 2; i < CHANNELS; ++i)
			std::cout << "#" << i << ": 0x" << hex3 << tracks[i].GetStreamSize() << ' ';
		std::cout << "Loop:        0x" << hex3 << loopTrack.GetStreamSize();
		std::cout << "\nSpace used by echo: 0x" << hex4 << (echoBufferSize << 11) <<
			" bytes.  Space used by samples: 0x" << hex4 << spaceUsedBySamples << " bytes.\n\n";
	}
	else {
		std::cout << name << ": ";		// // //
		for (int i = 0, n = 58 - name.size(); i < n; ++i)
			std::cout.put('.');
		std::cout.put(' ');

		if (knowsLength) {
			// int s = static_cast<int>((mainLength + introLength) / (2.0 * tempo) + 0.5);
			auto sec = static_cast<int>((introSeconds + mainSeconds + 0.5) / 60);		// // //
			std::cout << sec / 60 << ':' << std::setfill('0') << std::setw(2) << sec % 60;
		}
		else
			std::cout << "?:??";
		std::cout << ", 0x" << hex4 << totalSize << std::dec << " bytes\n";
	}

	if (totalSize > minSize && minSize > 0) {
		std::stringstream err;		// // //
		err << "Song was larger than it could pad out by 0x" << hex4 << totalSize - minSize << " bytes.";
		warn(err.str());		// // //
	}

	writeTextFile("stats" / fs::path {name}.stem().replace_extension(".txt"), [&] {
		std::stringstream statStrStream;

		for (const Track &t : tracks)		// // //
			statStrStream << "CHANNEL " << static_cast<int>(t.index) << " SIZE:				0x" << hex4 << t.GetStreamSize() << "\n";
		statStrStream << "LOOP DATA SIZE:				0x" << hex4 << loopTrack.GetStreamSize() << "\n";
		statStrStream << "POINTERS AND INSTRUMENTS SIZE:		0x" << hex4 << headerSize << "\n";
		statStrStream << "SAMPLES SIZE:				0x" << hex4 << spaceUsedBySamples << "\n";
		statStrStream << "ECHO SIZE:				0x" << hex4 << (echoBufferSize << 11) << "\n";
		statStrStream << "SONG TOTAL DATA SIZE:			0x" << hex4 << totalSize << "\n";		// // //

		if (index > highestGlobalSong)
			statStrStream << "FREE ARAM (APPROXIMATE):		0x" << hex4 << 0x10000 - (echoBufferSize << 11) - spaceUsedBySamples - totalSize - programUploadPos << "\n\n";
		else
			statStrStream << "FREE ARAM (APPROXIMATE):		UNKNOWN\n\n";

		for (const Track &t : tracks)		// // //
			statStrStream << "CHANNEL " << static_cast<int>(t.index) << " TICKS:			0x" << hex4 << static_cast<int>(t.channelLength) << "\n";
		statStrStream << '\n';

		if (knowsLength) {
			statStrStream << "SONG INTRO LENGTH IN SECONDS:		" << std::dec << introSeconds << "\n";
			statStrStream << "SONG MAIN LOOP LENGTH IN SECONDS:	" << mainSeconds << "\n";
			statStrStream << "SONG TOTAL LENGTH IN SECONDS:		" << introSeconds + mainSeconds << "\n";
		}
		else {
			statStrStream << "SONG INTRO LENGTH IN SECONDS:		UNKNOWN\n";
			statStrStream << "SONG MAIN LOOP LENGTH IN SECONDS:	UNKNOWN\n";
			statStrStream << "SONG TOTAL LENGTH IN SECONDS:		UNKNOWN\n";
		}

		return statStrStream.str();
	});
}

// // //
void Music::adjustHeaderPointers() {
	for (int j = 0, n = allPointersAndInstrs.size(); j < n; j += 2) {		// // //
		if (j == instrumentPos)		// // //
			j += instrumentData.size();

		auto it = allPointersAndInstrs.begin() + j;		// // //
		int temp = *it | (*(it + 1) << 8);

		if (temp == 0xFFFF)		// 0xFFFF = swap with 0x0000.
			assign_short(it, 0);		// // //
		else if (temp == 0xFFFE)	// 0xFFFE = swap with 0x00FF.
			assign_short(it, 0x00FF);		// // //
		else
			assign_short(it, temp + posInARAM);		// // //
	}
}

void Music::adjustLoopPointers() {
	int offset = posInARAM + getDataSize() + allPointersAndInstrs.size();
	for (Track &t : tracks)
		t.ShiftPointers(offset);
	loopTrack.ShiftPointers(offset);
}

// // //

void Music::parseSPCInfo() {
	using namespace AMKd::MML::Lexer;		// // //

	if (!GetParameters<Sep<'{'>>(mml_))
		throw AMKd::Utility::SyntaxException {"Could not find opening brace in SPC info command."};

	while (auto item = GetParameters<Sep<'#'>, Ident, QString>(mml_)) {
		const std::string &metaName = item.get<0>();
		std::string metaParam = item.get<1>();

		if (metaName == "length") {
			guessLength = (metaParam == "auto");
			if (!guessLength) {
				AMKd::MML::SourceView field {metaParam};		// // //
				auto param = AMKd::MML::Lexer::Time()(field);
				if (param && !field)
					seconds = requires(*param, 0u, 999u, "Songs longer than 16:39 are not allowed by the SPC format.");		// // //
				else
					throw AMKd::Utility::SyntaxException {"Error parsing SPC length field.  Format must be m:ss or \"auto\"."};		// // //
				knowsLength = true;
			}
			continue;
		}

		if (metaName == "dumper" && metaParam.size() > 16) {		// // //
			metaParam.erase(16);
			printWarning("#dumper field was too long.  Truncating to \"" + metaParam + "\".");		// // //
		}
		else if (metaParam.size() > 32) {
			metaParam.erase(32);
			printWarning('#' + metaName + " field was too long.  Truncating to \"" + metaParam + "\".");		// // //
		}

		if (metaName == "author")
			author = metaParam;
		else if (metaName == "comment")
			comment = metaParam;
		else if (metaName == "title")
			title = metaParam;
		else if (metaName == "game")
			game = metaParam;
		else if (metaName == "dumper")		// // //
			dumper = metaParam;
		else
			throw AMKd::Utility::ParamException("Unexpected type name found in SPC info command.  Only \"author\", \"comment\",\n"
												"\"title\", \"game\", and \"length\" are allowed.");
	}

	if (!GetParameters<Sep<'}'>>(mml_))
		throw AMKd::Utility::SyntaxException {"Unexpected end of SPC info command."};
}

void Music::addNoteLength(double ticks) {
	if (loopState2 != LoopType::none)		// // //
		(loopState2 == LoopType::sub ? superLoopLength : normalLoopLength) += ticks;
	else if (loopState1 != LoopType::none)
		(loopState1 == LoopType::sub ? superLoopLength : normalLoopLength) += ticks;
	else
		getActiveTrack().channelLength += ticks;		// // //
}

// // //
void Music::writeState(AMKd::Music::TrackState (AMKd::Music::Track::*state), int val) {
	getBaseTrack().*state = val;		// // //
	loopTrack.*state = val;
}

void Music::resetStates() {
	loopTrack.q = getActiveTrack().q;
	loopTrack.o = getActiveTrack().o;
	loopTrack.l = getActiveTrack().l;
}

void Music::synchronizeStates() {
	if (!inNormalLoop) {		// // //
		getActiveTrack().q.Update();
		getActiveTrack().o.Update();
		getActiveTrack().l.Update();
	}
	loopTrack.q.Update();
	loopTrack.o.Update();
	loopTrack.l.Update();
	getActiveTrack().lastDuration = 0;
}

int Music::divideByTempoRatio(int value, bool /*fractionIsError*/) {
	int temp = value / tempoRatio;
/*
	if (temp * tempoRatio != value)
		if (fractionIsError)
			printError("Using the tempo ratio on this value would result in a fractional value.", false, name, line);
		else
			warn("The tempo ratio resulted in a fractional value.");
*/
	return temp;
}

int Music::multiplyByTempoRatio(int value) {
	int temp = value * tempoRatio;
	if (temp > 0xFF) {		// // //		
		warn("Using the tempo ratio on this value would cause it to overflow.");
		temp = 0xFF;
	}
	return temp;
}

// // //
const std::string &Music::getFileName() const {
	return name;
}



// // //
void Music::doDirectWrite(int byte) {
	getActiveTrack().Append(AMKd::Binary::MakeByteChunk(byte));
}

void Music::doComment() {
//	if (songTargetProgram != Target::AMM)		// // //
//		throw AMKd::Utility::SyntaxException {"Illegal use of comments. Sorry about that. Should be fixed in AddmusicK 2."};		// // //
}

void Music::doBar() {
	// hexLeft = 0;
}

void Music::doExMark() {
	if (songTargetProgram != Target::AMK)		// // //
		mml_.Clear();
	else
		throw AMKd::Utility::SyntaxException {"The '!' directive is not supported by #amk."};
}

void Music::doNote(int note, int fullTicks, int bendTicks, bool nextPorta) {
	if (inRemoteDefinition)
		throw AMKd::Utility::SyntaxException {"Remote definitions cannot contain note data!"};
	if (songTargetProgram == Target::AMK && channelDefined == false && inRemoteDefinition == false)
		throw AMKd::Utility::SyntaxException {"Note data must be inside a channel!"};

	const int CHUNK_MAX_TICKS = 0x7F; // divideByTempoRatio(0x60, true);
	int flatTicks = fullTicks - bendTicks;
	if (flatTicks < 0)
		throw AMKd::Utility::Exception {"Something happened"};
	addNoteLength(flatTicks + bendTicks);
	if (bendTicks > CHUNK_MAX_TICKS) {
		warn("The pitch bend here will not fully span the note's duration from its last tie.");
		flatTicks += bendTicks % CHUNK_MAX_TICKS;
		bendTicks -= bendTicks % CHUNK_MAX_TICKS;
	}

	const auto doSingleNote = [this] (int note, int len) {		// // //
		if (getActiveTrack().q.NeedsUpdate()) {
			append(getActiveTrack().lastDuration = static_cast<uint8_t>(len));
			append(getActiveTrack().q.Get());
		}
		if (getActiveTrack().lastDuration != len)
			append(getActiveTrack().lastDuration = static_cast<uint8_t>(len));
		append(note);
	};

	const auto flushNote = [&] (int &note, int len) {		// // //
		const int tieNote = note == AMKd::Binary::CmdType::Rest ? note : AMKd::Binary::CmdType::Tie;
		if (len % 2 == 0 && len > CHUNK_MAX_TICKS && len <= 2 * CHUNK_MAX_TICKS) {
			doSingleNote(note, len / 2);
			doSingleNote(note = tieNote, len / 2);
		}
		else
			while (len) {
				int chunk = std::min(len, CHUNK_MAX_TICKS);
				doSingleNote(note, chunk);
				len -= chunk;
				note = tieNote;
			}
	};

	if (std::exchange(getActiveTrack().inPitchSlide, nextPorta))		// // //
		doPortamento(0, getActiveTrack().lastDuration, note);		// // //
	flushNote(note, flatTicks);
	flushNote(note, bendTicks);
}

void Music::doOctave(int oct) {
	return writeState(&Track::o, requires(oct, 1, 6, DIR_ILLEGAL("octave (\"o\")")));
}

void Music::doRaiseOctave() {
	int oct = getActiveTrack().o.Get();		// // //
	if (oct >= 6)
		throw AMKd::Utility::ParamException {"The octave has been raised too high."};
	return writeState(&Track::o, ++oct);
}

void Music::doLowerOctave() {
	int oct = getActiveTrack().o.Get();		// // //
	if (oct <= 1)
		throw AMKd::Utility::ParamException {"The octave has been dropped too low."};
	return writeState(&Track::o, --oct);
}

void Music::doVolume(int vol) {
	append(AMKd::Binary::CmdType::Vol, vol);
}

void Music::doVolume(int vol, int fadeLen) {
	append(AMKd::Binary::CmdType::VolFade, divideByTempoRatio(fadeLen, false), vol);
}

void Music::doGlobalVolume(int vol) {
	append(AMKd::Binary::CmdType::VolGlobal, vol);
}

void Music::doGlobalVolume(int vol, int fadeLen) {
	append(AMKd::Binary::CmdType::VolGlobalFade, divideByTempoRatio(fadeLen, false), vol);
}

void Music::doPan(int pan, bool sLeft, bool sRight) {
	append(AMKd::Binary::CmdType::Pan, pan | (sLeft << 7) | (sRight << 6));
}

void Music::doPanFade(int pan, int fadeLen) {
	append(AMKd::Binary::CmdType::PanFade, divideByTempoRatio(fadeLen, false), pan);
}

void Music::doEchoVolume(int left, int right, int mask) {
	append(AMKd::Binary::CmdType::EchoVol, mask, left, right);
}

void Music::doEchoParams(int delay, int reverb, int firIndex) {
	// Print error for AM4 songs that attempt to use an invalid FIR filter. They both
	// A) won't sound like their originals and
	// B) may crash the DSP (or for whatever reason that causes SPCPlayer to go silent with them).
	if (songTargetProgram == Target::AM4 && firIndex > 1)		// // //
		throw AMKd::Utility::ParamException {"Invalid FIR filter index for the $F1 command."};
	echoBufferSize = std::max(echoBufferSize, delay);
	append(AMKd::Binary::CmdType::EchoParams, delay, reverb, firIndex);
}

void Music::doEchoFade(int left, int right, int fadeLen) {
	append(AMKd::Binary::CmdType::EchoFade, divideByTempoRatio(fadeLen, false), left, right);
}

void Music::doEchoOff() {
	append(AMKd::Binary::CmdType::EchoOff);
}

void Music::doEchoToggle() {
	append(AMKd::Binary::CmdType::ExtF4, AMKd::Binary::CmdOptionF4::EchoToggle);
}

void Music::doInstrument(int inst, bool add) {
	if (add) {
		if (convert && inst >= 0x13 && inst < 30)	// Change it to an HFD custom instrument.
			inst = inst - 0x13 + 30;
		if (inst < 30)
			usedSamples[instrToSample[inst]] = true;
		else if ((inst - 30) * 6 < static_cast<int>(instrumentData.size()))		// // //
			usedSamples[instrumentData[(inst - 30) * 6]] = true;
		else if (optimizeSampleUsage)
			throw AMKd::Utility::ParamException {"This custom instrument has not been defined yet."};		// // //
		if (songTargetProgram == Target::AM4)		// // //
			getActiveTrack().ignoreTuning = false;
		append(AMKd::Binary::CmdType::Inst, inst);		// // //
	}
	if (inst < 30)		// // //
		usedSamples[instrToSample[inst]] = true;

	getActiveTrack().instrument = inst;		// // //
	/*
	hTranspose = 0;
	usingHTranspose = false;
	if (htranspose[inst])
		transposeMap[tracks[channe].instrument] = tmpTrans[tracks[channe].instrument];
	*/
}

void Music::doVibrato(int delay, int rate, int depth) {
	append(AMKd::Binary::CmdType::Vibrato,
		divideByTempoRatio(delay, false), multiplyByTempoRatio(rate), depth);
}

void Music::doVibratoOff() {
	append(AMKd::Binary::CmdType::VibratoOff);
}

void Music::doVibratoFade(int fadeLen) {
	append(AMKd::Binary::CmdType::VibratoFade, divideByTempoRatio(fadeLen, false));
}

void Music::doTremolo(int delay, int rate, int depth) {
	append(AMKd::Binary::CmdType::Tremolo,
		divideByTempoRatio(delay, false), multiplyByTempoRatio(rate), depth);
}

void Music::doTremoloOff() {
	append(AMKd::Binary::CmdType::Tremolo, 0x00, 0x00, 0x00);
}

void Music::doPortamento(int delay, int len, int target) {
	append(AMKd::Binary::CmdType::Portamento,
		   divideByTempoRatio(delay, false), divideByTempoRatio(len, false), target);
}

void Music::doPitchBend(int delay, int len, int target, bool bendAway) {
	append(bendAway ? AMKd::Binary::CmdType::BendAway : AMKd::Binary::CmdType::BendTo,
		   divideByTempoRatio(delay, false), divideByTempoRatio(len, false), target);
}

void Music::doDetune(int offset) {
	append(AMKd::Binary::CmdType::Detune, offset);
}

void Music::doEnvelope(int ad, int sr) {
	append(AMKd::Binary::CmdType::Envelope, ad, sr);
}

void Music::doFIRFilter(int f0, int f1, int f2, int f3, int f4, int f5, int f6, int f7) {
	append(AMKd::Binary::CmdType::FIR, f0, f1, f2, f3, f4, f5, f6, f7);
}

void Music::doNoise(int pitch) {
	append(AMKd::Binary::CmdType::Noise, pitch);
}

void Music::doTempo(int speed) {
	tempo = requires(divideByTempoRatio(speed, false), 0x01, 0xFF, "Tempo has been zeroed out by #halvetempo");		// // //
	append(AMKd::Binary::CmdType::Tempo, tempo);		// // //

	if (inNormalLoop || inSubLoop)		// // // Not even going to try to figure out tempo changes inside loops.  Maybe in the future.
		guessLength = false;
	else
		tempoChanges.emplace_back(getActiveTrack().channelLength, tempo);		// // //
}

void Music::doTempo(int speed, int fadeLen) {
	tempo = requires(divideByTempoRatio(speed, false), 0x01, 0xFF, "Tempo has been zeroed out by #halvetempo");		// // //
	guessLength = false; // NOPE.  Nope nope nope nope nope nope nope nope nope nope.
	append(AMKd::Binary::CmdType::TempoFade, divideByTempoRatio(fadeLen, false), tempo);
}

void Music::doTranspose(int offset) {
	append(AMKd::Binary::CmdType::ExtFA, AMKd::Binary::CmdOptionFA::Transpose, offset);
}

void Music::doTransposeGlobal(int offset) {
	if (songTargetProgram == Target::AM4)	// // // AM4 seems to do something strange with $E4?
		++offset;
	append(AMKd::Binary::CmdType::TrspGlobal, offset);
}

void Music::doSampleLoad(int id, int mult) {
	usedSamples[id] = true;		// // //
	append(AMKd::Binary::CmdType::SampleLoad, id, mult);		// // //
}

void Music::doLoopEnter() {
	prevLoop = static_cast<uint16_t>(loopTrack.GetStreamSize());		// // //
	synchronizeStates();		// // //
	if (std::exchange(inNormalLoop, true))		// // //
		throw AMKd::Utility::SyntaxException {"You cannot nest standard [ ] loops."};

	normalLoopLength = 0;

	if (inSubLoop) {					// We're entering a normal loop that's nested in a subloop
		loopState1 = LoopType::sub;		// // //
		loopState2 = LoopType::normal;
	}
	else {						// We're entering a normal loop that's not nested
		loopState1 = LoopType::normal;
		loopState2 = LoopType::none;
	}

	prevChannel = channel;				// We're in a loop now, which is represented as channel 8.
	loopTrack.instrument = getActiveTrack().instrument;		// // //
	if (songTargetProgram == Target::AM4)
		loopTrack.ignoreTuning = getActiveTrack().ignoreTuning; // More AM4 tuning stuff.  Related to the line above it.
	channel = CHANNELS;					// So we have to back up the current channel.
}

void Music::doLoopExit(int loopCount) {
	if (inRemoteDefinition)
		throw AMKd::Utility::SyntaxException {"Remote code definitions cannot repeat."};		// // //

	append(AMKd::Binary::CmdType::End);		// // //
	synchronizeStates();		// // //
	channel = prevChannel;

	if (!std::exchange(inNormalLoop, false))		// // //
		throw AMKd::Utility::SyntaxException {"Loop end found outside of a loop."};

	if (loopState2 == LoopType::normal) {				// We're leaving a normal loop that's nested in a subloop.
		loopState2 = LoopType::none;
		superLoopLength += normalLoopLength * loopCount;
	}
	else if (loopState1 == LoopType::normal) {			// We're leaving a normal loop that's not nested.
		loopState1 = LoopType::none;
		getActiveTrack().channelLength += normalLoopLength * loopCount;		// // //
	}

	if (loopLabel > 0)
		loopLengths[loopLabel] = normalLoopLength;

	if (!inRemoteDefinition) {
		getActiveTrack().AddLoopPosition(1);		// // //
		doLoopNative(prevLoop, loopCount);
	}
	inRemoteDefinition = false;
	loopLabel = 0;
	getActiveTrack().isDefiningLabelLoop = false;		// // //
}

void Music::doLoopLabel(int label) {
	if (loopPointers.find(++label) != loopPointers.cend())		// // //
		throw AMKd::Utility::ParamException {"Label redefinition."};
	loopPointers[label] = static_cast<uint16_t>(loopTrack.GetStreamSize());
}

void Music::doLoopRemoteCall(int loopCount, uint16_t loopAdr) {
	if (inNormalLoop)		// // //
		throw AMKd::Utility::SyntaxException {"You cannot nest standard [ ] loops."};		// // //

	synchronizeStates();		// // //
	addNoteLength((loopLabel ? loopLengths[loopLabel] : normalLoopLength) * loopCount);		// // //
	getActiveTrack().AddLoopPosition(1);		// // //
	return doLoopNative(loopAdr, loopCount);
}

void Music::doLoopNative(int addr, int loopCount) {
	append(AMKd::Binary::CmdType::Loop, addr & 0xFF, addr >> 8, loopCount);
}

void Music::doSubloopEnter() {
	if (loopLabel > 0 && getActiveTrack().isDefiningLabelLoop)		// // //
		throw AMKd::Utility::SyntaxException {"A label loop cannot define a subloop.  Use a standard or remote loop instead."};

	synchronizeStates();		// // //
	if (std::exchange(inSubLoop, true))		// // //
		throw AMKd::Utility::SyntaxException {"You cannot nest a subloop within another subloop."};

	superLoopLength = 0;

	if (inNormalLoop) {		// // // We're entering a subloop that's nested in a normal loop
		loopState1 = LoopType::normal;		// // //
		loopState2 = LoopType::sub;
	}
	else {						// We're entering a subloop that's not nested
		loopState1 = LoopType::sub;
		loopState2 = LoopType::none;
	}

	append(AMKd::Binary::CmdType::Subloop, 0x00);		// // //
}

void Music::doSubloopExit(int loopCount) {
	synchronizeStates();		// // //
	if (!std::exchange(inSubLoop, false))		// // //
		throw AMKd::Utility::SyntaxException {"A subloop end was found outside of a subloop."};

	if (loopState2 == LoopType::sub) {				// We're leaving a subloop that's nested in a normal loop.
		loopState2 = LoopType::none;
		normalLoopLength += superLoopLength * loopCount;
	}
	else if (loopState1 == LoopType::sub) {			// We're leaving a subloop that's not nested.
		loopState1 = LoopType::none;
		getActiveTrack().channelLength += superLoopLength * loopCount;		// // //
	}

	append(AMKd::Binary::CmdType::Subloop, loopCount - 1);		// // //
}

void Music::doRemoteCallNative(int addr, int type, int delay) {
	append(AMKd::Binary::CmdType::Callback, addr & 0xFF, addr >> 8, type, delay);
}

void Music::doIntro() {
	if (inNormalLoop)		// // //
		throw AMKd::Utility::SyntaxException {"Intro directive found within a loop."};		// // //

	if (!hasIntro)
		tempoChanges.emplace_back(getActiveTrack().channelLength, -static_cast<int>(tempo));		// // //
	else		// // //
		for (auto &x : tempoChanges)
			if (x.second < 0)
				x.second = -static_cast<int>(tempo);

	hasIntro = true;
	getActiveTrack().phrasePointers[1] = static_cast<uint16_t>(getActiveTrack().GetStreamSize());		// // //
	getActiveTrack().lastDuration = 0;
	introLength = static_cast<unsigned>(getActiveTrack().channelLength);		// // //
}

void Music::doYoshiDrums(bool ch5only) {
	append(AMKd::Binary::CmdType::ExtF4, ch5only ? AMKd::Binary::CmdOptionF4::YoshiCh5 : AMKd::Binary::CmdOptionF4::Yoshi);
	hasYoshiDrums = true;
}

void Music::doLegato() {
	append(AMKd::Binary::CmdType::ExtF4, AMKd::Binary::CmdOptionF4::Legato);
}

void Music::doLightStaccato() {
	append(AMKd::Binary::CmdType::ExtF4, AMKd::Binary::CmdOptionF4::LightStaccato);
}

void Music::doSync() {
	append(AMKd::Binary::CmdType::ExtF4, AMKd::Binary::CmdOptionF4::Sync);
}

void Music::doTempoImmunity() {
	append(AMKd::Binary::CmdType::ExtF4, AMKd::Binary::CmdOptionF4::TempoImmunity);
}

void Music::doVolumeTable(bool louder) {
	if (louder)
		append(AMKd::Binary::CmdType::ExtF4, AMKd::Binary::CmdOptionF4::VelocityTable);
	else
		append(AMKd::Binary::CmdType::ExtFA, AMKd::Binary::CmdOptionFA::VolTable, /*louder ? 0x01 :*/ 0x00);
}

void Music::doRestoreInst() {
	append(AMKd::Binary::CmdType::ExtF4, AMKd::Binary::CmdOptionF4::RestoreInst);
}

void Music::doPitchMod(int flag) {
	append(AMKd::Binary::CmdType::ExtFA, AMKd::Binary::CmdOptionFA::PitchMod, flag);
}

void Music::doGain(int gain) {
	append(AMKd::Binary::CmdType::ExtFA, AMKd::Binary::CmdOptionFA::Gain, gain);
}

void Music::doAmplify(int mult) {
	append(AMKd::Binary::CmdType::ExtFA, AMKd::Binary::CmdOptionFA::Amplify, mult);
}

void Music::doEchoBuffer(int size) {
	append(AMKd::Binary::CmdType::ExtFA, AMKd::Binary::CmdOptionFA::EchoBuffer, size);
	echoBufferSize = std::max(echoBufferSize, size);
}

void Music::doDSPWrite(int adr, int val) {
	append(AMKd::Binary::CmdType::DSP, adr, val);
}

void Music::doARAMWrite(int /*adr*/, int /*val*/) {
	throw AMKd::Utility::MMLException {"ARAM writes are disabled by default."};
//	append(AMKd::Binary::CmdType::ARAM, val, adr >> 8, adr);
}

void Music::doDataSend(int val1, int val2) {
	append(AMKd::Binary::CmdType::DataSend, val1, val2);
}

void Music::doArpeggio(int dur, const std::vector<uint8_t> &notes) {
	getActiveTrack().Append(AMKd::Binary::ChunkAMK::Arpeggio(notes.size(), divideByTempoRatio(dur, false)));
	getActiveTrack().Append(AMKd::Binary::ListChunk(notes));
}

void Music::doTrill(int dur, int offset) {
	getActiveTrack().Append(AMKd::Binary::ChunkAMK::Trill(divideByTempoRatio(dur, false), offset));
}

void Music::doGlissando(int dur, int offset) {
	getActiveTrack().Append(AMKd::Binary::ChunkAMK::Glissando(divideByTempoRatio(dur, false), offset));
}

void Music::doTriplet(bool enable) {
	if (enable == std::exchange(getActiveTrack().inTriplet, enable))		// // //
		throw AMKd::Utility::ParamException {"Triplet block mismatch."};
//		throw AMKd::Utility::SyntaxException {"Triplet on directive found within a triplet block."};
}
