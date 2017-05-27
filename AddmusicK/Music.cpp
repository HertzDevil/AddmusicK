#include "Music.h"
//#include "globals.h"
//#include "Sample.h"

#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <locale>		// // //

#include "globals.h"		// // //
#include "Utility/Exception.h"		// // //
#include "Binary/Defines.h"		// // //
#include "MML/Lexer.h"		// // //
#include "MML/Preprocessor.h"		// // //
#include <functional>

//#include "Preprocessor.h"


// // //

#define CMD_ERROR(name, abbr) "Error parsing " name " (\"" abbr "\") command."
#define CMD_ILLEGAL(name, abbr) "Illegal value for " name " (\"" abbr "\") command."
#define DIR_ERROR(name) "Error parsing " name " directive."
#define DIR_ILLEGAL(name) "Illegal value for " name " directive."

template <typename T, typename U, typename V>
decltype(auto) apply_this(T &&fn, U pObj, V &&tup) {
	return std::apply(std::forward<T>(fn), std::tuple_cat(std::make_tuple(pObj), std::forward<V>(tup)));
}

template <typename T, typename U>
T requires(T x, T min, T max, U&& err) {
	return (x >= min && x <= max) ? x :
		throw AMKd::Utility::ParamException {std::forward<U>(err)};
}

#define warn(str) if (false); else \
	::printWarning(str, name, mml_.GetLineNumber())

#define error(str) do { \
		::printError(str, name, mml_.GetLineNumber()); \
		return; \
	} while (false)

// // //
void Music::fatalError(const std::string &str) {
	::fatalError(str, name, mml_.GetLineNumber());		// // //
}

static int channel, prevChannel, octave, prevNoteLength, defaultNoteLength;
static bool inDefineBlock;
static bool inNormalLoop;		// // //

static unsigned int prevLoop;
static bool doesntLoop;
static bool triplet;
static bool inPitchSlide;

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
static int hTranspose;
static bool usingHTranspose;

//static int tempLoopLength;		// How long the current [ ] loop is.
//static int e6LoopLength;		// How long the current $E6 loop is.
//static int previousLoopLength;	// How long the last encountered loop was.
static bool inE6Loop;			// Whether or not we're in an $E6 loop.
static int loopNestLevel;		// How deep we're "loop nested".
// If this is 0, then any type of loop is allowed.
// If this is 1 and we're in a normal loop, then normal loops are disallowed and $E6 loops are allowed.
// If this is 1 and we're in an $E6 loop, then $E6 loops are disallowed and normal loops are allowed.
// If this is 2, then no new loops are allowed.

//static unsigned int lengths[CHANNELS];		// How long each channel is.

static unsigned int tempo;
//static bool onlyHadOneTempo;
static bool tempoDefined;

static bool manualNoteWarning;

static bool channelDefined;
//static int am4silence;			// Used to keep track of the brief silence at the start of am4 songs.

//static bool normalLoopInsideE6Loop;
//static bool e6LoopInsideNormalLoop;

static std::string basepath;

static bool usingSMWVTable;

//static int hTranspose = 0;00

// // // vs2017 does not have fold-expressions
#define SWALLOW(x) do { \
		using _swallow = int[]; \
		(void)_swallow {0, ((x), 0)...}; \
	} while (false)



// // //
template <typename... Args>
void Music::append(Args&&... value) {
#if 0
	(tracks[::channel].data.push_back(static_cast<uint8_t>(value)), ...);
#else
	SWALLOW(tracks[::channel].data.push_back(static_cast<uint8_t>(value)));
#endif
}

// // //
bool Music::trimChar(char c) {
	return mml_.Trim(c).has_value();
}

// // //
bool Music::trimDirective(std::string_view str) {
	return mml_.Trim(str, true).has_value();
}

// // //
void Music::skipSpaces() {
	mml_.SkipSpaces();
}

Music::Music() {
	knowsLength = false;
	totalSize = 0;
	spaceForPointersAndInstrs = 0;
	echoBufferSize = 0;
}

void Music::init() {
	basepath = "./";
	prevChannel = 0;
	manualNoteWarning = true;
	tempoDefined = false;
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

	superLoopLength = normalLoopLength = 0;

	baseLoopIsNormal = baseLoopIsSuper = extraLoopIsNormal = extraLoopIsSuper = false;
	// // //

	inE6Loop = false;
	seconds = 0;

	hTranspose = 0;
	usingHTranspose = false;
	// // //
	octave = 4;
	prevNoteLength = -1;
	defaultNoteLength = 8;

	prevLoop = -1;
	// // //

	inDefineBlock = false;

	hasIntro = false;
	doesntLoop = false;
	triplet = false;
	inPitchSlide = false;

	loopLabel = 0;
	// // //

	for (int z = 0; z < 256; z++) {
		transposeMap[z] = 0;
		usedSamples[z] = false;
	}

	for (int z = 0; z < 19; z++)
		transposeMap[z] = tmpTrans[z];

	title = name.substr(0, name.find_last_of('.'));		// // //
	size_t p = name.find_last_of('/');
	if (p != std::string::npos)
		title = name.substr(p + 1);
	p = name.find_last_of('\\');
	if (p != std::string::npos)
		title = name.substr(p);

	//std::string backup = text;

	const auto stat = AMKd::MML::Preprocessor {openTextFile(fs::path {"music"} / name), name};		// // //
	mml_ = AMKd::MML::SourceFile {stat.result};
	songTargetProgram = stat.target;		// // //
	targetAMKVersion = stat.version;
	if (!stat.title.empty())
		title = stat.title;

	switch (stat.target) {		// // //
	case Target::AMM:
		break;
	case Target::AM4:
		for (auto &t : tracks)		// // //
			t.ignoreTuning = true; // AM4 fix for tuning[] related stuff.
		break;
	case Target::AMK:
		/*
		targetAMKVersion = 0;
		if (backup.find('\r') != -1)
			backup = backup.insert(backup.length(), "\r\n\r\n#amk=1\r\n");		// Automatically assume that this is a song written for AMK.
		else
			backup = backup.insert(backup.length(), "\n\n#amk=1\n");
		writeTextFile(fs::path {"music"} / name, backup);
		*/
		if (targetAMKVersion > PARSER_VERSION)
			error("This song was made for a newer version of AddmusicK.  You must update to use\nthis song.");
		break;
	default:
		error("Song did not specify target program with #amk, #am4, or #amm.");
	}

	usingSMWVTable = (targetAMKVersion < 2);		// // //
	
	// // // We can't just insert this at the end due to looping complications and such.
	if (validateHex && index > highestGlobalSong && stat.firstChannel != CHANNELS) {
		channel = stat.firstChannel;
		if (targetAMKVersion > 1)
			doVolumeTable(true);
		append(AMKd::Binary::CmdType::ExtFA, AMKd::Binary::CmdOptionFA::EchoBuffer, echoBufferSize);
	}

	channel = 0;
}

void Music::compile() {
	static const auto CMDS = [] {		// // //
		AMKd::Utility::Trie<void (Music::*)()> cmd;

		cmd.Insert("p", &Music::parseVibratoCommand);
		cmd.Insert("tuning", &Music::parseTransposeDirective);
		cmd.Insert("t", &Music::parseTempoCommand);
		cmd.Insert("[", &Music::parseLoopCommand);
		cmd.Insert("[[", &Music::parseSubloopCommand);
		cmd.Insert("[[[", &Music::parseErrorLoopCommand);
		cmd.Insert("]", &Music::parseLoopEndCommand);
		cmd.Insert("]]", &Music::parseSubloopEndCommand);
		cmd.Insert("]]]", &Music::parseErrorLoopEndCommand);
		cmd.Insert("*", &Music::parseStarLoopCommand);
		cmd.Insert("o", &Music::parseOctaveDirective);
		cmd.Insert(">", &Music::parseRaiseOctaveDirective);
		cmd.Insert("<", &Music::parseLowerOctaveDirective);
		cmd.Insert("v", &Music::parseVolumeCommand);

		return cmd;
	}();

	init();

	try {
		while (hasNextToken()) {		// // //
			if (auto token = mml_.ExtractToken(CMDS)) {		// // //
				(this->*(*token))();
				continue;
			}

			char ch = ::tolower((*mml_.Trim("."))[0]);		// // //

			switch (ch) {
			case '?': parseQMarkDirective();		break;
//			case '!': parseExMarkDirective();		break;
			case '#': parseChannelDirective();		break;
			case 'l': parseLDirective();			break;
			case 'w': parseGlobalVolumeCommand();	break;
			case 'q': parseQuantizationCommand();	break;
			case 'y': parsePanCommand();			break;
			case '/': parseIntroDirective();		break;
			case '@': parseInstrumentCommand();		break;
			case '(': parseOpenParenCommand();		break;
			case '{': parseTripletOpenDirective();	break;
			case '}': parseTripletCloseDirective();	break;
			case '$': parseHexCommand();			break;
			case 'h': parseHDirective();			break;
			case 'n': parseNCommand();				break;
			case '"': parseReplacementDirective();	break;
			case '|':								break;		// // // no-op
			case 'c': case 'd': case 'e': case 'f': case 'g': case 'a': case 'b': case 'r': case '^':
				parseNote(ch);						break;
			case ';': parseComment();				break;		// Needed for comments in quotes
			default:
				throw AMKd::Utility::SyntaxException {"Unexpected character \"" + std::string(1, ch) + "\" found."};
			}
		}
	}
	catch (AMKd::Utility::SyntaxException &e) {
		fatalError(e.what());
	}
	catch (AMKd::Utility::MMLException &e) {
		error(e.what());
	}

	pointersFirstPass();
}

// // //
size_t Music::getDataSize() const {
	size_t x = 0;
	for (int i = 0; i < CHANNELS; ++i)
		x += tracks[i].data.size();
	return x;
}

// // //
void Music::FlushSongData(std::vector<uint8_t> &buf) const {
	buf.reserve(buf.size() + getDataSize() + tracks[CHANNELS].data.size() + allPointersAndInstrs.size());
	buf.insert(buf.end(), allPointersAndInstrs.cbegin(), allPointersAndInstrs.cend());
	for (const auto &x : tracks)
		buf.insert(buf.cend(), x.data.cbegin(), x.data.cend());
}

void Music::parseComment() {
	if (songTargetProgram == Target::AMM)		// // //
		mml_.Trim(".*?\\n");
	else
		error("Illegal use of comments. Sorry about that. Should be fixed in AddmusicK 2.");		// // //
}

void Music::printChannelDataNonVerbose(int totalSize) {
	std::cout << name << ": ";		// // //
	for (int i = 0, n = 58 - name.size(); i < n; ++i)
		std::cout.put('.');
	std::cout.put(' ');

	if (knowsLength) {
		// int s = static_cast<int>((mainLength + introLength) / (2.0 * tempo) + 0.5);
		auto seconds = static_cast<int>((introSeconds + mainSeconds + 0.5) / 60);		// // //
		std::cout << seconds / 60 << ':' << std::setfill('0') << std::setw(2) << seconds % 60;
	}
	else
		std::cout << "?:??";
	std::cout << ", 0x" << hex4 << totalSize << std::dec << " bytes\n";
}

void Music::parseQMarkDirective() {
	using namespace AMKd::MML::Lexer;
	if (auto param = GetParameters<Int>(mml_)) {
		switch (param.get<0>()) {
		case 0: doesntLoop = true; break;
		case 1: tracks[channel].noMusic[0] = true; break;		// // //
		case 2: tracks[channel].noMusic[1] = true; break;
		default:
			error(DIR_ERROR("\"?\""));
		}
	}
	else
		doesntLoop = true;
}

void Music::parseExMarkDirective() {
	mml_.Clear();
}

void Music::parseChannelDirective() {
	if (isalpha(peek())) {		// // //
		parseSpecialDirective();
		return;
	}

	int i = getInt();		// // //
	if (i != -1) {
		channel = requires(i, 0, static_cast<int>(CHANNELS) - 1, DIR_ILLEGAL("channel"));
		tracks[CHANNELS].q = tracks[channel].q;		// // //
		tracks[CHANNELS].updateQ = tracks[channel].updateQ;
		prevNoteLength = -1;

		hTranspose = 0;
		usingHTranspose = false;
		channelDefined = true;
		/*for (int u = 0; u < CHANNELS * 2; u++)
		{
		if (htranspose[u])			// Undo what the h directive did.
		transposeMap[u] = tmpTrans[u];
		}*/
		//hTranspose = 0;
		return;
	}

	error(DIR_ERROR("channel"));
}

void Music::parseLDirective() {
	using namespace AMKd::MML::Lexer;		// // //
	if (auto param = GetParameters<Int>(mml_))
		defaultNoteLength = requires(param.get<0>(), 1u, 192u, DIR_ILLEGAL("note length (\"l\")"));		// // //
	else
		error(DIR_ERROR("note length (\"l\")"));
}

void Music::parseGlobalVolumeCommand() {
	using namespace AMKd::MML::Lexer;		// // //
	if (auto param = GetParameters<Int>(mml_))
		return doGlobalVolume(requires(param.get<0>(), 0x00u, 0xFFu, CMD_ILLEGAL("global volume", "w")));		// // //
	error(CMD_ERROR("global volume", "w"));		// // //
}

void Music::parseVolumeCommand() {
	using namespace AMKd::MML::Lexer;		// // //
	if (auto param = GetParameters<Int>(mml_))
		return doVolume(requires(param.get<0>(), 0x00u, 0xFFu, CMD_ILLEGAL("volume", "v")));
	error(CMD_ERROR("volume", "v"));		// // //
}

void Music::parseQuantizationCommand() {
	int i = getHex();		// // //
	if (i != -1) {
		int q = requires(i, 0x01, 0x7F, CMD_ILLEGAL("quantization", "q"));		// // //

		if (inNormalLoop) {		// // //
			tracks[prevChannel].q = q;		// // //
			tracks[prevChannel].updateQ = true;
		}
		else {
			tracks[channel].q = q;
			tracks[channel].updateQ = true;
		}

		tracks[CHANNELS].q = q;
		tracks[CHANNELS].updateQ = true;
		return;
	}

	error(CMD_ERROR("quantization", "q"));		// // //
}

void Music::parsePanCommand() {
	using namespace AMKd::MML::Lexer;
	if (auto param = GetParameters<Int>(mml_)) {
		unsigned pan = requires(param.get<0>(), 0u, 20u, CMD_ILLEGAL("pan", "y"));
		if (auto panParam = GetParameters<Comma, Int, Comma, Int>(mml_)) {
			if (requires(panParam.get<0>(), 0u, 1u, CMD_ILLEGAL("pan", "y")))
				pan |= 1 << 7;
			if (requires(panParam.get<1>(), 0u, 1u, CMD_ILLEGAL("pan", "y")))
				pan |= 1 << 6;
		}
		return append(AMKd::Binary::CmdType::Pan, pan);		// // //
	}

	error(CMD_ERROR("pan", "y"));		// // //
}

void Music::parseIntroDirective() {
	if (inNormalLoop)		// // //
		error("Intro directive found within a loop.");		// // //

	if (!hasIntro)
		tempoChanges.emplace_back(tracks[channel].channelLength, -static_cast<int>(tempo));		// // //
	else		// // //
		for (auto &x : tempoChanges)
			if (x.second < 0)
				x.second = -static_cast<int>(tempo);

	hasIntro = true;
	tracks[channel].phrasePointers[1] = static_cast<uint16_t>(tracks[channel].data.size());		// // //
	prevNoteLength = -1;
	introLength = static_cast<unsigned>(tracks[channel].channelLength);		// // //
}

void Music::parseTempoCommand() {
	using namespace AMKd::MML::Lexer;		// // //
	if (auto param = GetParameters<Int>(mml_))
		return doTempo(requires(param.get<0>(), 0x00u, 0xFFu, CMD_ILLEGAL("tempo", "t")));
	error(CMD_ERROR("tempo", "t"));
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

	error(DIR_ERROR("tuning"));
}

void Music::parseOctaveDirective() {
	using namespace AMKd::MML::Lexer;		// // //
	if (auto param = GetParameters<Int>(mml_))
		return doOctave(param.get<0>());
	error(DIR_ERROR("octave (\"o\")"));
}

void Music::parseInstrumentCommand() {
	bool direct = trimChar('@');		// // //

	int i = getInt();		// // //
	if (i != -1) {
		i = requires(i, 0x00, 0xFF, CMD_ILLEGAL("instrument", "@"));

		if ((i <= 18 || direct) || i >= 30) {
			if (convert)
				if (i >= 0x13 && i < 30)	// Change it to an HFD custom instrument.
					i = i - 0x13 + 30;

			if (optimizeSampleUsage)
				if (i < 30)
					usedSamples[instrToSample[i]] = true;
				else if ((i - 30) * 6 < static_cast<int>(instrumentData.size()))		// // //
					usedSamples[instrumentData[(i - 30) * 6]] = true;
				else
					error("This custom instrument has not been defined yet.");		// // //

			if (songTargetProgram == Target::AM4)		// // //
				tracks[channel].ignoreTuning = false;

			append(AMKd::Binary::CmdType::Inst, i);		// // //
		}

		if (i < 30)
			if (optimizeSampleUsage)
				usedSamples[instrToSample[i]] = true;

		//hTranspose = 0;
		//usingHTranspose = false;
		tracks[channel].instrument = i;		// // //
		//if (htranspose[i] == true)
		//	transposeMap[tracks[channe].instrument] = ::tmpTrans[tracks[channe].instrument];
		return;
	}

	error(CMD_ERROR("instrument", "@"));		// // //
}

void Music::parseOpenParenCommand() {
	int sampID;
	using namespace AMKd::MML::Lexer;		// // //
	if (auto param = GetParameters<Sep<'@'>, Int>(mml_))
		sampID = instrToSample[requires(param.get<0>(), 0u, 29u, "Illegal instrument number for sample load command.")];
	else if (auto param = GetParameters<Sep<'\"'>, QString>(mml_)) {
		std::string s = basepath + param.get<0>();		// // //
		auto it = std::find(mySamples.cbegin(), mySamples.cend(), getSample(s, this));		// // //
		if (it == mySamples.cend())
			error("The specified sample was not included in this song.");
		sampID = std::distance(mySamples.cbegin(), it);
	}
	else
		return parseLabelLoopCommand();

	if (auto ext = GetParameters<Comma, Byte, Sep<')'>>(mml_))
		return doSampleLoad(sampID, ext.get<0>());
	error("Error parsing sample load command.");
}

void Music::parseLabelLoopCommand() {
	using namespace AMKd::MML::Lexer;
	if (trimChar('!')) {		// // //
		if (targetAMKVersion < 2)
			error("Unrecognized character '!'.");

		if (channelDefined) {						// A channel's been defined, we're parsing a remote 
			auto param = GetParameters<Int, Comma, SInt>(mml_);		// // //
			if (!param)
				error("Error parsing remote code setup.");

			int i = param.get<0>();
			int j = param.get<1>();
			int k = 0;
			if (j == AMKd::Binary::CmdOptionFC::Sustain || j == AMKd::Binary::CmdOptionFC::Release) {
				if (!GetParameters<Comma>(mml_))
					error("Error parsing remote code setup. Missing the third argument.");
				if (auto len = GetParameters<Byte>(mml_))
					k = len.get<0>();
				else if (auto param = GetParameters<Dur>(mml_)) {		// // //
					k = getRawTicks(param.get<0>());
					if (k > 0x100)
						error("Note length specified was too large.");		// // //
					else if (k == 0x100)
						k = 0;
				}
				else
					error("Error parsing remote code setup.");
			}

			if (!GetParameters<Sep<')'>, Sep<'['>>(mml_))
				error("Error parsing remote code setup.");

			tracks[channel].loopLocations.push_back(static_cast<uint16_t>(tracks[channel].data.size() + 1));		// // //
			if (loopPointers.find(i) == loopPointers.cend())		// // //
				loopPointers.insert({i, -1});
			append(AMKd::Binary::CmdType::Callback, loopPointers[i] & 0xFF, loopPointers[i] >> 8, j, k);
			return;
		}

		int i = getInt();		// // // We're outside of a channel, this is a remote call definition.
		if (i == -1)
			error("Error parsing remote code definition.");
		if (skipSpaces(), !trimChar(')'))		// // //
			error("Error parsing remote code definition.");
		if (trimChar('['))		// // //
			error("Error parsing remote code definition; the definition was missing.");

		loopLabel = i;		// // //
		inRemoteDefinition = true;
		return parseLoopCommand();
	}

	if (inNormalLoop)		// // //
		error("Nested loops are not allowed.");		// // //

	int i = getInt();		// // //
	if (i == -1)
		error("Error parsing label loop.");		// // //
	if (++i >= 0x10000)		// Needed to allow for songs that used label 0.
		error("Illegal value for loop label.");		// // //

	if (!trimChar(')'))		// // //
		error("Error parsing label loop.");

	synchronizeQ();		// // //
	loopLabel = i;

	if (trimChar('[')) {				// If this is a loop definition...
		tracks[channel].isDefiningLabelLoop = true;		// // // The rest of the code is handled in the respective function.
		return parseLoopCommand();
	}

	auto it = loopPointers.find(loopLabel);		// // // Otherwise, if this is a loop call...
	if (it == loopPointers.cend())
		error("Label not yet defined.");
	int j = getInt();		// // //
	if (j == -1)
		j = 1;
	if (j < 1 || j > 255)
		error("Invalid loop count.");		// // //

	doLoopRemoteCall(j);

	tracks[channel].loopLocations.push_back(static_cast<uint16_t>(tracks[channel].data.size() + 1));		// // //
	append(AMKd::Binary::CmdType::Loop, it->second & 0xFF, it->second >> 8, j);

	loopLabel = 0;
}

void Music::parseLoopCommand() {
	if (inNormalLoop)		// // //
		error("You cannot nest standard [ ] loops.");
	synchronizeQ();		// // //

	prevLoop = tracks[CHANNELS].data.size();		// // //

	prevChannel = channel;				// We're in a loop now, which is represented as channel 8.
	channel = CHANNELS;					// So we have to back up the current channel.
	inNormalLoop = true;		// // //
	tracks[CHANNELS].instrument = tracks[prevChannel].instrument;		// // //
	if (songTargetProgram == Target::AM4)
		tracks[CHANNELS].ignoreTuning = tracks[prevChannel].ignoreTuning; // More AM4 tuning stuff.  Related to the line above it.

	if (loopLabel > 0 && loopPointers.find(loopLabel) != loopPointers.cend())		// // //
		error("Label redefinition.");

	if (loopLabel > 0)
		loopPointers[loopLabel] = prevLoop;

	doLoopEnter();
}

// // //
void Music::parseSubloopCommand() {
	if (loopLabel > 0 && tracks[channel].isDefiningLabelLoop)		// // //
		error("A label loop cannot define a subloop.  Use a standard or remote loop instead.");
	return doSubloopEnter();
}

// // //
void Music::parseErrorLoopCommand() {
	fatalError("An ambiguous use of the [ and [[ loop delimiters was found (\"[[[\").  Separate\n"
	           "the \"[[\" and \"[\" to clarify your intention.");
}

void Music::parseLoopEndCommand() {
	if (!inNormalLoop)		// // //
		error("Loop end found outside of a loop.");
	synchronizeQ();		// // //

	int i = getInt();		// // //
	if (i == -1)
		i = 1;
	else if (inRemoteDefinition)
		error("Remote code definitions cannot repeat.");		// // //
	if (i < 1 || i > 255)
		error("Invalid loop count.");

	append(0);
	channel = prevChannel;
	inNormalLoop = false;		// // //

	doLoopExit(i);

	if (!inRemoteDefinition) {
		tracks[channel].loopLocations.push_back(static_cast<uint16_t>(tracks[channel].data.size() + 1));		// // //
		append(AMKd::Binary::CmdType::Loop, prevLoop & 0xFF, prevLoop >> 8, i);
	}
	inRemoteDefinition = false;
	loopLabel = 0;
	tracks[channel].isDefiningLabelLoop = false;		// // //
}

// // //
void Music::parseSubloopEndCommand() {
	using namespace AMKd::MML::Lexer;		// // //
	if (auto param = GetParameters<Int>(mml_))
		return doSubloopExit(requires(param.get<0>(), 2u, 256u, CMD_ILLEGAL("subloop", "[[ ]]")));
	error(CMD_ERROR("subloop", "[[ ]]"));
}

// // //
void Music::parseErrorLoopEndCommand() {
	fatalError("An ambiguous use of the ] and ]] loop delimiters was found (\"]]]\").  Separate\n"
	           "the \"]]\" and \"]\" to clarify your intention.");
}

void Music::parseStarLoopCommand() {
	if (inNormalLoop)		// // //
		error("Nested loops are not allowed.");		// // //
	synchronizeQ();		// // //
	
	using namespace AMKd::MML::Lexer;		// // //
	if (auto param = GetParameters<Opt<Int>>(mml_)) {
		int count = 1;
		if (auto l = param.get<0>())
			count = requires(*l, 0x01u, 0xFFu, CMD_ILLEGAL("star loop", "*"));
		doLoopRemoteCall(count);
		tracks[channel].loopLocations.push_back(static_cast<uint16_t>(tracks[channel].data.size() + 1));		// // //
		append(AMKd::Binary::CmdType::Loop, prevLoop & 0xFF, prevLoop >> 8, count);
		return;
	}
	error(CMD_ERROR("star loop", "*"));
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

	error(CMD_ERROR("vibrato", "p"));		// // //
}

void Music::parseTripletOpenDirective() {
	if (triplet)		// // //
		error("Triplet on directive found within a triplet block.");
	triplet = true;
}

void Music::parseTripletCloseDirective() {
	if (!triplet)		// // //
		error("Triplet off directive found outside of a triplet block.");
	triplet = false;
}

void Music::parseRaiseOctaveDirective() {
	return doOctave(octave + 1);		// // //
}

void Music::parseLowerOctaveDirective() {
	return doOctave(octave - 1);		// // //
}

void Music::parseHFDInstrumentHack(int addr, int bytes) {
	int byteNum = 0;
	do {
		int i;		// // //
		if (!getHexByte(i))		// // //
			error("Unknown HFD hex command.");
		instrumentData.push_back(i);
		byteNum++;
		addr++;
		if (byteNum == 1) {
			if (optimizeSampleUsage)
				usedSamples[i] = true;
		}
		if (byteNum == 5) {
			instrumentData.push_back(0);	// On the 5th byte, we need to add a 0 as the new sub-multiplier.
			byteNum = 0;
		}
		bytes--;
	} while (bytes >= 0);
}

void Music::parseHFDHex() {
	int i;		// // //
	if (!getHexByte(i))		// // //
		error("Unknown HFD hex command.");

	if (convert) {
		using namespace AMKd::MML::Lexer;
		switch (i) {
		case 0x80:
			if (auto param = GetParameters<Byte, Byte>(mml_)) {		// // //
				unsigned reg, val;
				std::tie(reg, val) = *param;
				if (reg == 0x6D || reg == 0x7D) // Do not write the HFD header hex bytes.
					songTargetProgram = Target::AM4; // The HFD header bytes indicate this as being an AM4 song, so it gets AM4 treatment.
				else if (reg == 0x6C) // Noise command gets special treatment.
					append(AMKd::Binary::CmdType::Noise, val);
				else
					append(AMKd::Binary::CmdType::DSP, reg, val);		// // //
				return;
			}
			break;
		case 0x81:
			if (auto param = GetParameters<Byte, Byte>(mml_)) {		// // //
				append(AMKd::Binary::CmdType::ExtFA, AMKd::Binary::CmdOptionFA::Transpose, param.get<0>());		// // //
				return;
			}
			break;
		case 0x82:
			if (auto param = GetParameters<Byte, Byte, Byte, Byte>(mml_)) {		// // //
				int addr = (param.get<0>() << 8) | param.get<1>();
				int bytes = (param.get<2>() << 8) | param.get<3>();
				if (addr == 0x6136)
					return parseHFDInstrumentHack(addr, bytes);
				while (bytes-- >= 0) {		// // //
					if (!GetParameters<Byte>(mml_))
						error("Error while parsing HFD hex command.");
					// Don't do this stuff; we won't know what we're overwriting.
					// append(AMKd::Binary::CmdType::ARAM, i, addr >> 8, addr & 0xFF);		// // //
					++addr;
				}
				return;
			}
			break;
		}
		if (i >= 0x80)		// // //
			error("Error while parsing HFD hex command.");
	}

	append(AMKd::Binary::CmdType::Envelope, i);		// // //
}

// // //
void Music::insertRemoteConversion(uint8_t cmdtype, uint8_t param, std::vector<uint8_t> &&cmd) {
	auto index = static_cast<uint16_t>(tracks[channel].data.size() + 1);
	tracks[channel].remoteGainInfo.emplace_back(index, std::move(cmd));		// // //
	append(AMKd::Binary::CmdType::Callback, 0x00, 0x00, cmdtype, param);		// // //
}

static bool nextNoteIsForDD;

void Music::parseHexCommand() {
	auto hex = AMKd::MML::Lexer::Hex<2>()(mml_);		// // //
	if (!hex)
		error("Error parsing hex command.");
	int i = *hex;

	const auto getval = [&] {		// // // TODO: remove
		using namespace AMKd::MML::Lexer;
		auto param = GetParameters<Byte>(mml_);
		if (!param)
			error("Error parsing hex command.");
		i = param.get<0>();
	};

	if (!validateHex) {
		append(i);
		return;
	}

	using namespace AMKd::MML::Lexer;		// // //
	switch (int currentHex = i) {		// // //
	case AMKd::Binary::CmdType::Inst:
		append(currentHex);
		getval();
		if (songTargetProgram == Target::AM4) {		// // // If this was the instrument command
			if (i >= 0x13)					// And it was >= 0x13
				i = i - 0x13 + 30;		// Then change it to a custom instrument.
			if (optimizeSampleUsage)
				usedSamples[instrumentData[(i - 30) * 5]] = true;
		}
		append(i);
		return;
	case AMKd::Binary::CmdType::Pan:
		append(currentHex);
		getval();
		append(i);
		return;
	case AMKd::Binary::CmdType::PanFade:
		append(currentHex);
		getval();
		append(divideByTempoRatio(i, false));
		getval();
		append(i);
		return;
	case AMKd::Binary::CmdType::Portamento:
		append(currentHex);
		getval();
		append(divideByTempoRatio(i, false));
		getval();
		{		// Hack allowing the $DD command to accept a note as a parameter.
			AMKd::MML::SourceFile checkNote {mml_};		// // //
			while (true) {
				checkNote.SkipSpaces();
				if (checkNote.Trim("[A-Ga-g]")) {
					if (tracks[channel].updateQ)		// // //
						error("You cannot use a note as the last parameter of the $DD command if you've also\n"
								"used the qXX command just before it.");		// // //
					nextNoteIsForDD = true;
					break;
				}
				else if (checkNote.Trim('o'))
					AMKd::MML::Lexer::GetParameters<AMKd::MML::Lexer::Int>(checkNote);
				else if (!checkNote.Trim('<') && !checkNote.Trim('>'))
					break;
			}
			i = divideByTempoRatio(i, false);
		}
		append(i);
		if (!nextNoteIsForDD) {
			getval();
			append(i);
		}
		return;
	case AMKd::Binary::CmdType::Vibrato:
		if (auto param = GetParameters<Byte, Byte, Byte>(mml_))
			return apply_this(&Music::doVibrato, this, *param);
		error("Unknown hex command.");
	case AMKd::Binary::CmdType::VibratoOff:
		append(currentHex);
		return;
	case AMKd::Binary::CmdType::VolGlobal:
		if (auto param = GetParameters<Byte>(mml_))
			return apply_this(&Music::doGlobalVolume, this, *param);
		error("Unknown hex command.");
	case AMKd::Binary::CmdType::VolGlobalFade:
		append(currentHex);
		getval();
		append(divideByTempoRatio(i, false));
		getval();
		append(i);
		return;
	case AMKd::Binary::CmdType::Tempo:
		if (auto param = GetParameters<Byte>(mml_))
			return apply_this(&Music::doTempo, this, *param);
		error("Unknown hex command.");
	case AMKd::Binary::CmdType::TempoFade:
		guessLength = false; // NOPE.  Nope nope nope nope nope nope nope nope nope nope.
		append(currentHex);
		getval();
		append(divideByTempoRatio(i, false));
		getval();
		append(i);
		return;
	case AMKd::Binary::CmdType::TrspGlobal:
		append(currentHex);
		getval();
		if (songTargetProgram == Target::AM4)	// // // AM4 seems to do something strange with $E4?
			++i &= 0xFF;
		append(i);
		return;
	case AMKd::Binary::CmdType::Tremolo:
		if (auto first = GetParameters<Byte>(mml_)) {		// // //
			if (songTargetProgram == Target::AM4 && first.get<0>() >= 0x80) {
				if (mySamples.empty() && (i & 0x7F) > 0x13)
					error("This song uses custom samples, but has not yet defined its samples with the #samples command.");		// // //
				auto param = GetParameters<Byte>(mml_);
				return doSampleLoad(i - 0x80, param.get<0>());
			}
			if (auto remain = GetParameters<Byte, Byte>(mml_))
				return apply_this(&Music::doTremolo, this, std::tuple_cat(*first, *remain));
		}
		error("Unknown hex command.");
	case AMKd::Binary::CmdType::Subloop:
		if (songTargetProgram == Target::AM4)		// // // N-SPC tremolo off
			return doTremoloOff();		// // //
		if (auto param = GetParameters<Byte>(mml_)) {
			int repeats = param.get<0>();
			return repeats ? doSubloopExit(repeats + 1) : doSubloopEnter();
		}
		error("Unknown hex command.");
	case AMKd::Binary::CmdType::Vol:
		if (auto param = GetParameters<Byte>(mml_))
			return apply_this(&Music::doVolume, this, *param);
		error("Unknown hex command.");
	case AMKd::Binary::CmdType::VolFade:
		append(currentHex);
		getval();
		append(divideByTempoRatio(i, false));
		getval();
		append(i);
		return;
	case AMKd::Binary::CmdType::Loop:
		append(currentHex);
		getval();
		append(i);
		getval();
		append(i);
		getval();
		append(i);
		return;
	case AMKd::Binary::CmdType::VibratoFade:
		append(currentHex);
		getval();
		append(divideByTempoRatio(i, false));
		return;
	case AMKd::Binary::CmdType::BendAway:
	case AMKd::Binary::CmdType::BendTo:
		append(currentHex);
		getval();
		append(divideByTempoRatio(i, false));
		getval();
		append(divideByTempoRatio(i, false));
		getval();
		append(i);
		return;
	case AMKd::Binary::CmdType::Envelope:
		if (songTargetProgram == Target::AM4) {
			parseHFDHex();
			getval();
			append(i);
			return;
		}
		if (auto param = GetParameters<Byte, Byte>(mml_))
			return apply_this(&Music::doEnvelope, this, *param);
		error("Unknown hex command.");
	case AMKd::Binary::CmdType::Detune:
		append(currentHex);
		getval();
		append(i);
		return;
	case AMKd::Binary::CmdType::Echo1:
		append(currentHex);
		getval();
		append(i);
		getval();
		append(i);
		getval();
		append(i);
		return;
	case AMKd::Binary::CmdType::EchoOff:
		append(currentHex);
		return;
	case AMKd::Binary::CmdType::Echo2:
		append(currentHex);
		getval();
		append(i);
		echoBufferSize = std::max(echoBufferSize, i);
		getval();
		append(i);
		getval();
		// Print error for AM4 songs that attempt to use an invalid FIR filter. They both
		// A) won't sound like their originals and
		// B) may crash the DSP (or for whatever reason that causes SPCPlayer to go silent with them).
		if (songTargetProgram == Target::AM4) {		// // //
			if (i > 1) {
				std::stringstream ss;		// // //
				ss << '$' << hex2 << i;
				error(ss.str() + " is not a valid FIR filter for the $F1 command. Must be either $00 or $01.");
			}
		}
		append(i);
		return;
	case AMKd::Binary::CmdType::EchoFade:
		append(currentHex);
		getval();
		append(divideByTempoRatio(i, false));
		getval();
		append(i);
		getval();
		append(i);
		return;
	case AMKd::Binary::CmdType::SampleLoad:
		if (auto param = GetParameters<Byte, Byte>(mml_))
			return apply_this(&Music::doSampleLoad, this, *param);
		error("Unknown hex command.");
	case AMKd::Binary::CmdType::ExtF4:
		append(currentHex);
		getval();
		append(i);
		if (i == AMKd::Binary::CmdOptionF4::YoshiCh5 || i == AMKd::Binary::CmdOptionF4::Yoshi)
			hasYoshiDrums = true;
		return;
	case AMKd::Binary::CmdType::FIR:
		append(currentHex);
		for (int j = 0; j < 8; ++j) {
			getval();
			append(i);
		}
		return;
	case AMKd::Binary::CmdType::DSP:
		append(currentHex);
		getval();
		append(i);
		getval();
		append(i);
		return;
	case AMKd::Binary::CmdType::ARAM:
		append(currentHex);
		getval();
		append(i);
		getval();
		append(i);
		getval();
		append(i);
		return;
	case AMKd::Binary::CmdType::Noise:
		append(currentHex);
		getval();
		append(i);
		return;
	case AMKd::Binary::CmdType::DataSend:
		append(currentHex);
		getval();
		append(i);
		getval();
		append(i);
		return;
	case AMKd::Binary::CmdType::ExtFA:
		getval();
		// // // More code conversion.
		if (targetAMKVersion == 1 && i == 0x05) {
			//if (tempoRatio != 1) error("#halvetempo cannot be used on AMK 1 songs that use the $FA $05 or old $FC command.")
			int channelToCheck = inNormalLoop ? prevChannel : channel;

			getval();
			tracks[channelToCheck].usingFA = (i != 0);		// // //

			if (tracks[channelToCheck].usingFA)
				if (tracks[channelToCheck].usingFC)
					insertRemoteConversion(0x05, tracks[channelToCheck].lastFCDelayValue,
						{AMKd::Binary::CmdType::ExtFA, AMKd::Binary::CmdOptionFA::Gain, static_cast<uint8_t>(i), 0x00});
				else {
					insertRemoteConversion(AMKd::Binary::CmdOptionFC::KeyOn, 0x00,
						{AMKd::Binary::CmdType::ExtF4, AMKd::Binary::CmdOptionF4::RestoreInst, 0x00});
					insertRemoteConversion(AMKd::Binary::CmdOptionFC::KeyOff, 0x00,
						{AMKd::Binary::CmdType::ExtFA, AMKd::Binary::CmdOptionFA::Gain, static_cast<uint8_t>(i), 0x00});
				}
			else
				insertRemoteConversion(AMKd::Binary::CmdOptionFC::Disable, 0x00, {});
			return;
		}
		if (targetAMKVersion > 1 && i == 0x05)
			error("$FA $05 in #amk 2 or above has been replaced with remote code.");		// // //
		append(currentHex);
		append(i);
		getval();
		append(i);
		return;
	case AMKd::Binary::CmdType::Arpeggio:
		if (auto param = GetParameters<Byte, Byte>(mml_)) {
			unsigned count, len;
			std::tie(count, len) = *param;
			append(AMKd::Binary::CmdType::Arpeggio, count, divideByTempoRatio(len, false));
			if (count >= 0x80) {
				if (count != 0x80 && count != 0x81)
					error("Illegal value for arpeggio command.");
				count = 2;
			}
			for (unsigned i = 0; i < count; ++i) {
				auto note = GetParameters<Byte>(mml_);
				if (!note)
					error("Error parsing arpeggio command.");
				append(note.get<0>());
			}
			return;
		}
		error("Unknown hex command.");
	case AMKd::Binary::CmdType::Callback:
		if (targetAMKVersion == 1) {
			//if (tempoRatio != 1) error("#halvetempo cannot be used on AMK 1 songs that use the $FA $05 or old $FC command.")
			int channelToCheck = inNormalLoop ? prevChannel : channel;		// // //

			getval();
			tracks[channelToCheck].usingFC = (i != 0);
			if (tracks[channelToCheck].usingFC) {
				tracks[channelToCheck].lastFCDelayValue = static_cast<uint8_t>(divideByTempoRatio(i, false));		// // //

				getval();
				if (tracks[channelToCheck].usingFA)
					insertRemoteConversion(0x05, tracks[channelToCheck].lastFCDelayValue,
						{AMKd::Binary::CmdType::ExtFA, AMKd::Binary::CmdOptionFA::Gain, static_cast<uint8_t>(i), 0x00});
				else {
					insertRemoteConversion(AMKd::Binary::CmdOptionFC::KeyOn, 0x00,
						{AMKd::Binary::CmdType::ExtF4, AMKd::Binary::CmdOptionF4::RestoreInst, 0x00});
					insertRemoteConversion(AMKd::Binary::CmdOptionFC::Release, tracks[channelToCheck].lastFCDelayValue,
						{AMKd::Binary::CmdType::ExtFA, AMKd::Binary::CmdOptionFA::Gain, static_cast<uint8_t>(i), 0x00});
				}
			}
			else {
				getval();
				if (tracks[channelToCheck].usingFA)
					insertRemoteConversion(AMKd::Binary::CmdOptionFC::KeyOff, 0x00,
						{AMKd::Binary::CmdType::ExtFA, AMKd::Binary::CmdOptionFA::Gain, static_cast<uint8_t>(i), 0x00}); // check this
				else
					insertRemoteConversion(AMKd::Binary::CmdOptionFC::Disable, 0x00, {});
			}
			return;
		}
			
		if (targetAMKVersion > 1)
			error("$FC has been replaced with remote code in #amk 2 and above.");		// // //
		append(currentHex);
		getval();
		append(i);
		getval();
		append(i);
		getval();
		append(i);
		getval();
		append(i);
		return;
	case 0xFD: case 0xFE: case 0xFF:
		error("Unknown hex command.");
	default:
		if (currentHex < AMKd::Binary::CmdType::Inst && manualNoteWarning) {
			if (targetAMKVersion == 0) {
				warn("Warning: A hex command was found that will act as a note instead of a special\n"
						"effect. If this is a song you're using from someone else, you can most likely\n"
						"ignore this message, though it may indicate that a necessary #amm or #am4 is\n"
						"missing.");		// // //
				manualNoteWarning = false;
				return;
			}
			error("Unknown hex command.");
		}
	}
}

void Music::parseNote(int ch) {		// // //
	if (inRemoteDefinition)
		error("Remote definitions cannot contain note data!");

	// // //
	if (songTargetProgram == Target::AMK && channelDefined == false && inRemoteDefinition == false)
		error("Note data must be inside a channel!");

	int i;		// // //
	bool isRest = false;		// // //
	if (ch == 'r') {
		i = AMKd::Binary::CmdType::Rest;		// // //
		isRest = true;
	}
	else if (ch == '^')
		i = AMKd::Binary::CmdType::Tie;
	else {
		//am4silence++;
		i = getPitch(ch);

		if (usingHTranspose)
			i += hTranspose;
		else
			if (!tracks[channel].ignoreTuning)		// // // More AM4 tuning stuff
				i -= transposeMap[tracks[channel].instrument];

		if (i < 0x80)		// // //
			error("Note's pitch was too low.");
		if (i >= AMKd::Binary::CmdType::Tie)
			error("Note's pitch was too high.");
		if (tracks[channel].instrument >= 21 && tracks[channel].instrument < 30) {		// // //
			i = 0xD0 + (tracks[channel].instrument - 21);

			if ((channel == 6 || channel == 7 || (inNormalLoop && (prevChannel == 6 || prevChannel == 7))) == false)	// If this is not a SFX channel,
				tracks[channel].instrument = 0xFF;										// Then don't force the drum pitch on every note.
		}
	}

	if (inPitchSlide) {
		inPitchSlide = false;
		append(AMKd::Binary::CmdType::Portamento, 0x00, prevNoteLength, i);		// // //
	}

	if (nextNoteIsForDD) {
		append(i);
		nextNoteIsForDD = false;
		return;
	}

	using namespace AMKd::MML::Lexer;		// // //
	AMKd::MML::Duration dur = isRest ? GetParameters<RestDur>(mml_).get<0>() : GetParameters<Dur>(mml_).get<0>();

	inPitchSlide = GetParameters<Sep<'&'>>(mml_).success();
	int bendTicks = inPitchSlide ? getLastTicks(dur) : 0;
	if (!inPitchSlide && mml_.Trim("\\$DD", true)) {
		mml_.Unput();
		bendTicks = getLastTicks(dur);
	}

	const int CHUNK_MAX_TICKS = 0x7F; // divideByTempoRatio(0x60, true);
	int j = getFullTicks(dur) - bendTicks;
	if (j < 0)
		fatalError("Something happened");
	addNoteLength(j + bendTicks);
	if (bendTicks > CHUNK_MAX_TICKS) {
		warn("The pitch bend here will not fully span the note's duration from its last tie.");
		j += bendTicks % CHUNK_MAX_TICKS;
		bendTicks -= bendTicks % CHUNK_MAX_TICKS;
	}

	const auto doNote = [this] (int note, int len) {		// // //
		if (prevNoteLength != len)
			append(prevNoteLength = len);
		else if (tracks[channel].updateQ)
			append(len);
		if (tracks[channel].updateQ) {
			append(tracks[channel].q);
			tracks[channel].updateQ = tracks[CHANNELS].updateQ = false;
		}
		append(note);
	};

	const int tieNote = i == AMKd::Binary::CmdType::Rest ? i : AMKd::Binary::CmdType::Tie;
	while (j) {
		int chunk = std::min(j, CHUNK_MAX_TICKS);
		doNote(i, chunk);
		j -= chunk;
		i = tieNote;
	}
	while (bendTicks) {
		int chunk = std::min(bendTicks, CHUNK_MAX_TICKS);
		doNote(i, chunk);
		bendTicks -= chunk;
		i = tieNote;
	}
}

void Music::parseHDirective() {
	using namespace AMKd::MML::Lexer;		// // //
	if (auto param = GetParameters<SInt>(mml_)) {
		hTranspose = param.get<0>();
		usingHTranspose = true;
		//transposeMap[tracks[channel].instrument] = -param.get<0>();		// // //
		//htranspose[tracks[channel].instrument] = true;
		return;
	}
	error(DIR_ERROR("transpose (\"h\")"));
}

void Music::parseNCommand() {
	int i = getHex();		// // //
	if (i < 0 || i > 0x1F)
		error("Invlid value for the n command.  Value must be in hex and between 0 and 1F.");		// // //

	append(AMKd::Binary::CmdType::Noise, i);		// // //
}

void Music::parseOptionDirective() {
	if (targetAMKVersion == 1)
		error("Unknown command.");

	if (channelDefined == true)
		error("#option directives must be used before any and all channels.");		// // //

	skipSpaces();

	if (trimDirective("smwvtable"))		// // //
		if (!usingSMWVTable) {
			doVolumeTable(false);
			usingSMWVTable = true;
		}
		else
			warn("This song is already using the SMW V Table. This command is just wasting three bytes...");		// // //
	else if (trimDirective("nspcvtable")) {		// // //
		doVolumeTable(true);
		usingSMWVTable = false;
		warn("This song uses the N-SPC V by default. This command is just wasting two bytes...");		// // //
	}
	else if (trimDirective("tempoimmunity"))		// // //
		append(AMKd::Binary::CmdType::ExtF4, AMKd::Binary::CmdOptionF4::TempoImmunity);		// // //
	else if (trimDirective("noloop"))		// // //
		doesntLoop = true;
	else if (trimDirective("dividetempo")) {		// // //
		int i = getInt();		// // //
		if (i == -1)
			error("Missing integer argument for #option dividetempo.");		// // //
		if (i == 0)
			error("Argument for #option dividetempo cannot be 0.");		// // //
		if (i == 1)
			warn("#option dividetempo 1 has no effect besides printing this warning.");		// // //
		if (i < 0)
			error("#halvetempo has been used too many times...what are you even doing?");		// // //

		tempoRatio = i;
	}
	else
		error("#option directive missing its first argument");		// // //
}

void Music::parseSpecialDirective() {
	if (trimDirective("instruments"))		// // //
		parseInstrumentDefinitions();
	else if (trimDirective("samples"))		// // //
		parseSampleDefinitions();
	else if (trimDirective("pad"))		// // //
		parsePadDefinition();
	else if (trimDirective("spc"))		// // //
		parseSPCInfo();
	else if (trimDirective("louder")) {		// // //
		if (targetAMKVersion > 1)
			printWarning("#louder is redundant in #amk 2 and above.");
		parseLouderCommand();
	}
	else if (trimDirective("tempoimmunity"))		// // //
		append(AMKd::Binary::CmdType::ExtF4, AMKd::Binary::CmdOptionF4::TempoImmunity);		// // //
	else if (trimDirective("path"))		// // //
		parsePath();
	else if (trimDirective("halvetempo")) {		// // //
		if (channelDefined)
			error("#halvetempo must be used before any and all channels.");		// // //
		tempoRatio *= 2;

		if (tempoRatio < 0)
			error("#halvetempo has been used too many times...what are you even doing?");		// // //
	}
	else if (trimDirective("option"))		// // //
		parseOptionDirective();
	else
		error("Unknown option type for '#' directive.");		// // //
}

void Music::parseReplacementDirective() {
	std::string s = getEscapedString();		// // //
	int i = s.find('=');
	if (i == std::string::npos)
		fatalError("Error parsing replacement directive; could not find '='");		// // //

	std::string findStr = s.substr(0, i);
	std::string replStr = s.substr(i + 1);

	// // //
	while (!findStr.empty() && isspace(findStr.back()))
		findStr.pop_back();
	if (findStr.empty())
		fatalError("Error parsing replacement directive; string to find was of zero length.");

	while (!replStr.empty() && isspace(replStr.front()))
		replStr.erase(0, 1);

	mml_.AddMacro(findStr, replStr);		// // //
}

void Music::parseInstrumentDefinitions() {
	if (skipSpaces(), !trimChar('{'))		// // //
		fatalError("Could not find opening curly brace in instrument definition.");

	while (skipSpaces(), !trimChar('}')) {
		int i;		// // //
		if (trimChar('\"')) {		// // //
			std::string brrName = getEscapedString();
			if (brrName.empty())
				fatalError("Error parsing sample portion of the instrument definition.");
			brrName = basepath + brrName;
			int gs = getSample(brrName, this);
			auto it = std::find(mySamples.cbegin(), mySamples.cend(), getSample(brrName, this));		// // //
			if (it == mySamples.cend())
				fatalError("The specified sample was not included in this song.");		// // //
			i = std::distance(mySamples.cbegin(), it);
		}
		else if (trimChar('n')) {		// // //
			i = getHex();
			if (i == -1 || i > 0xFF)
				fatalError("Error parsing the noise portion of the instrument command.");
			if (i >= 0x20)
				fatalError("Invalid noise pitch.  The value must be a hex value from 0 - 1F.");		// // //

			i |= 0x80;
		}
		else if (trimChar('@')) {
			i = getInt();
			if (i == -1)
				fatalError("Error parsing the instrument copy portion of the instrument command.");
			if (i >= 30)
				fatalError("Cannot use a custom instrument's sample as a base for another custom instrument.");		// // //

			i = instrToSample[i];
		}
		else
			fatalError("Error parsing instrument definition.");

		instrumentData.push_back(i);
		if (optimizeSampleUsage)
			usedSamples[i] = true;

		for (int j = 0; j < 5; j++) {
			if (!getHexByte(i))		// // //
				fatalError("Error parsing instrument definition; there were too few bytes following the sample (there must be 6).");
			instrumentData.push_back(i);
		}
	}

	/*
	enum parseState
	{
		lookingForOpenBrace,
		lookingForAnything,
		lookingForDollarSign,
		lookingForOpenQuote,
		gettingName,
		gettingValue,
	};

	parseState state = lookingForOpenBrace;

	//unsigned char temp;
	int count = 0;

	while (pos < text.length()) {
		switch (state) {
		case lookingForOpenBrace:
			if (isspace(peek())) break;
			if (peek() != '{')
				error("Could not find opening curly brace in instrument definition.");
			state = lookingForDollarSign;
			break;
		case lookingForDollarSign:
			if (peek() == '\n')
				count = 0;
			if (isspace(peek())) break;
			if (peek() == '$') {
				if (count == 6) error("Invalid number of arguments for instrument.  Total number of bytes must be a multiple of 6.");
				state = gettingValue;
				break;
			}
			if (peek() == '}') {
				if (count != 0)
					error("Invalid number of arguments for instrument.  Total number of bytes must be a multiple of 6.");
				skipChars(1);
				return;
			}

			error("Error parsing instrument definition.");
			break;
		case gettingValue:
			int val = getHex();
			if (val == -1 || val > 255) error("Error parsing instrument definition.");
			instrumentData.push_back(val);
			state = lookingForDollarSign;
			count++;
			break;
		}
		skipChars(1);
	}
	*/
}

void Music::parseSampleDefinitions() {
	if (skipSpaces(), !trimChar('{'))		// // //
		fatalError("Unexpected character in sample group definition.  Expected \"{\".");

	while (true) {		// // //
		if (skipSpaces(), trimChar('\"')) {		// // //
			std::string tempstr = basepath + getEscapedString();		// // //
			auto tmppos = tempstr.find_last_of(".");
			if (tmppos == -1)
				fatalError("The filename for the sample was missing its extension; is it a .brr or .bnk?");		// // //
			std::string extension = tempstr.substr(tmppos);
			if (extension == ".bnk")
				addSampleBank(tempstr, this);
			else if (extension == ".brr")
				addSample(tempstr, this, false);
			else
				fatalError("The filename for the sample was invalid.  Only \".brr\" and \".bnk\" are allowed.");		// // //
		}
		else if (trimChar('#'))
			addSampleGroup(getIdentifier(), this);		// // //
		else if (trimChar('}'))
			return;
		else
			fatalError("Unexpected end of sample group definition.");
	}
}

void Music::parsePadDefinition() {
	if (hasNextToken() && trimChar('$')) {		// // //
		using namespace AMKd::MML::Lexer;
		if (auto param = HexInt()(mml_)) {
			minSize = *param;
			return;
		}
	}

	error(DIR_ERROR("padding"));		// // //
}

void Music::parseLouderCommand() {
	append(AMKd::Binary::CmdType::ExtF4, AMKd::Binary::CmdOptionF4::VelocityTable);		// // //
}

void Music::parsePath() {
	if (skipSpaces(), !trimChar('\"'))
		fatalError("Unexpected symbol found in path command. Expected a quoted string.");
	auto str = getEscapedString();		// // //
	if (str.empty())
		fatalError("Unexpected symbol found in path command. Expected a quoted string.");
	basepath = "./" + str + "/";
}

// // //
bool Music::hasNextToken() {
	return mml_.HasNextToken();
}

// // //
int Music::peek() {
	return mml_.Peek();
}

int Music::getInt() {
	//if (peek() == '$') { skipChars(1); return getHex(); }	// Allow for things such as t$20 instead of t32.
	// Actually, can't do it.
	// Consider this:
	// l8r$ED$00$00
	// Yeah. Oh well.
	// Attempt to do a replacement.  (So things like "ab = 8"    [c1]ab    are valid).
	if (hasNextToken())		// // //
		if (auto param = AMKd::MML::Lexer::Int()(mml_))
			return *param;
	return -1;
}

int Music::getIntWithNegative() {
	if (hasNextToken())		// // //
		if (auto param = AMKd::MML::Lexer::SInt()(mml_))
			return *param;
	throw "Invalid number";
}

int Music::getHex() {		// // //
	if (hasNextToken())		// // //
		if (auto param = AMKd::MML::Lexer::Hex<2>()(mml_))
			return *param;
	return -1;
}

// // //
bool Music::getHexByte(int &out) {
	if (hasNextToken())		// // //
		if (auto param = AMKd::MML::Lexer::Byte()(mml_)) {
			out = *param;
			return true;
		}
	return false;
}

int Music::getPitch(int i) {
	static const int pitches[] = {9, 11, 0, 2, 4, 5, 7};

	i = pitches[i - 'a'] + (octave - 1) * 12 + 0x80;		// // //
	if (trimChar('+'))
		++i;
	else if (trimChar('-'))
		--i;

	/*if (i < 0x80)
	return -1;
	if (i >= 0xC6)
	return -2;*/

	return i;
}

// // //
int Music::getRawTicks(const AMKd::MML::Duration &dur) const {
	return checkTickFraction(dur.GetTicks() / tempoRatio);
}

int Music::getFullTicks(const AMKd::MML::Duration &dur) const {
	return checkTickFraction(dur.GetTicks(defaultNoteLength) / tempoRatio * (triplet ? 2. / 3. : 1.));
}

int Music::getLastTicks(const AMKd::MML::Duration &dur) const {
	return checkTickFraction(dur.GetLastTicks(defaultNoteLength) / tempoRatio * (triplet ? 2. / 3. : 1.));
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
std::string Music::getIdentifier() {
	if (auto param = AMKd::MML::Lexer::Ident()(mml_))
		return *param;
	printError("Error while parsing identifier.");		// // //
	return "";
}

// // //
std::string Music::getEscapedString() {
	if (auto param = AMKd::MML::Lexer::QString()(mml_))
		return *param;
	printError("Unexpected end of file found.");		// // //
	return "";
}

// // //

void Music::pointersFirstPass() {
	if (errorCount)
		fatalError("There were errors when compiling the music file.  Compilation aborted.  Your ROM has not been modified.");

	if (std::all_of(tracks, tracks + CHANNELS, [] (const Track &t) { return t.data.empty(); }))		// // //
		error("This song contained no musical data!");

	if (targetAMKVersion == 1)			// Handle more conversion of the old $FC command to remote call.
		for (auto &track : tracks) for (const auto &x : track.remoteGainInfo) {		// // //
			uint16_t dataIndex = x.first;
			track.loopLocations.push_back(dataIndex);

			track.data[dataIndex] = static_cast<uint8_t>(tracks[CHANNELS].data.size() & 0xFF);
			track.data[dataIndex + 1] = static_cast<uint8_t>(tracks[CHANNELS].data.size() >> 8);

			tracks[CHANNELS].data.insert(tracks[CHANNELS].data.end(), x.second.cbegin(), x.second.cend());
		}

	for (int z = 0; z < CHANNELS; z++) if (!tracks[z].data.empty())		// // //
		tracks[z].data.push_back(AMKd::Binary::CmdType::End);

	if (mySamples.empty())		// // // If no sample groups were provided...
		addSampleGroup("default", this);		// // //

	if (game.empty())
		game = "Super Mario World (custom)";

	if (optimizeSampleUsage) {
		int emptySampleIndex = ::getSample("EMPTY.brr", this);
		if (emptySampleIndex == -1) {
			addSample("EMPTY.brr", this, true);
			emptySampleIndex = getSample("EMPTY.brr", this);
		}

		for (size_t z = 0, n = mySamples.size(); z < n; ++z)		// // //
			if (usedSamples[z] == false && samples[mySamples[z]].important == false)
				mySamples[z] = emptySampleIndex;
	}

	int binpos = 0;		// // //
	for (int i = 0; i < CHANNELS; ++i) {
		if (!tracks[i].data.empty())
			tracks[i].phrasePointers[0] = binpos;
		tracks[i].phrasePointers[1] += tracks[i].phrasePointers[0];
		binpos += tracks[i].data.size();
	}
	// // //
	spaceForPointersAndInstrs = 20;

	if (hasIntro)
		spaceForPointersAndInstrs += 18;
	if (!doesntLoop)
		spaceForPointersAndInstrs += 2;

	spaceForPointersAndInstrs += instrumentData.size();

	allPointersAndInstrs.resize(spaceForPointersAndInstrs);
	//for (i = 0; i < spaceForPointers; i++) allPointers[i] = 0x55;

	int add = (hasIntro ? 2 : 0) + (doesntLoop ? 0 : 2) + 4;

	//memcpy(allPointersAndInstrs.data() + add, instrumentData.base, instrumentData.size());
	for (size_t z = 0, n = instrumentData.size(); z < n; ++z)		// // //
		allPointersAndInstrs[add + z] = instrumentData[z];

	allPointersAndInstrs[0] = static_cast<uint8_t>((add + instrumentData.size()) & 0xFF);		// // //
	allPointersAndInstrs[1] = static_cast<uint8_t>((add + instrumentData.size()) >> 8);

	if (doesntLoop) {
		allPointersAndInstrs[add - 2] = 0xFF;	// Will be re-evaluated to 0000 when the pointers are adjusted later.
		allPointersAndInstrs[add - 1] = 0xFF;
	}
	else {
		allPointersAndInstrs[add - 4] = 0xFE;	// Will be re-evaluated to FF00 when the pointers are adjusted later.
		allPointersAndInstrs[add - 3] = 0xFF;
		if (hasIntro) {
			allPointersAndInstrs[add - 2] = 0xFD;	// Will be re-evaluated to 0002 + ARAMPos when the pointers are adjusted later.
			allPointersAndInstrs[add - 1] = 0xFF;
		}
		else {
			allPointersAndInstrs[add - 2] = 0xFC;	// Will be re-evaluated to ARAMPos when the pointers are adjusted later.
			allPointersAndInstrs[add - 1] = 0xFF;
		}
	}
	if (hasIntro) {
		allPointersAndInstrs[2] = static_cast<uint8_t>((add + instrumentData.size() + 16) & 0xFF);		// // //
		allPointersAndInstrs[3] = static_cast<uint8_t>((add + instrumentData.size() + 16) >> 8);
	}

	add += instrumentData.size();
	for (int i = 0; i < CHANNELS; ++i) {		// // //
		unsigned short adr = tracks[i].data.empty() ? 0xFFFB : (tracks[i].phrasePointers[0] + spaceForPointersAndInstrs);
		allPointersAndInstrs[i * 2 + 0 + add] = static_cast<uint8_t>(adr & 0xFF);		// // //
		allPointersAndInstrs[i * 2 + 1 + add] = static_cast<uint8_t>(adr >> 8);
	}

	if (hasIntro) {
		for (int i = 0; i < CHANNELS; ++i) {		// // //
			unsigned short adr = tracks[i].data.empty() ? 0xFFFB : (tracks[i].phrasePointers[1] + spaceForPointersAndInstrs);
			allPointersAndInstrs[i * 2 + 16 + add] = static_cast<uint8_t>(adr & 0xFF);		// // //
			allPointersAndInstrs[i * 2 + 17 + add] = static_cast<uint8_t>(adr >> 8);
		}
	}

	totalSize = spaceForPointersAndInstrs + tracks[CHANNELS].data.size() + getDataSize();		// // //

	//if (tempo == -1) tempo = 0x36;
	unsigned int totalLength;
	mainLength = -1;
	for (int i = 0; i < CHANNELS; i++)
		if (tracks[i].channelLength != 0)		// // //
			mainLength = std::min(mainLength, (unsigned int)tracks[i].channelLength);
	if (mainLength == -1)
		error("This song doesn't seem to have any data.");		// // //

	totalLength = mainLength;

	if (hasIntro)
		mainLength -= introLength;

	if (guessLength) {
		double l1 = 0, l2 = 0;
		bool onL1 = true;

		std::sort(tempoChanges.begin(), tempoChanges.end());		// // //
		if (tempoChanges.empty() || tempoChanges[0].first != 0) {
			tempoChanges.insert(tempoChanges.begin(), std::make_pair(0., 0x36)); // Stick the default tempo at the beginning if necessary.
		}

		tempoChanges.emplace_back(totalLength, 0);		// // // Add in a dummy tempo change at the very end.

		for (size_t z = 0, n = tempoChanges.size() - 1; z < n; ++z)		// // //
		{
			if (tempoChanges[z].first > totalLength) {
				warn("A tempo change was found beyond the end of the song.");		// // //
				break;
			}

			if (tempoChanges[z].second < 0)
				onL1 = false;

			double difference = tempoChanges[z + 1].first - tempoChanges[z].first;
			if (onL1)
				l1 += difference / (2 * std::abs(tempoChanges[z].second));
			else
				l2 += difference / (2 * std::abs(tempoChanges[z].second));
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

	int spaceUsedBySamples = 0;
	for (const uint16_t x : mySamples)		// // //
		spaceUsedBySamples += 4 + samples[x].data.size();	// The 4 accounts for the space used by the SRCN table.

	if (verbose)
		std::cout << name << " total size: 0x" << hex4 << totalSize << std::dec << " bytes\n";
	else
		printChannelDataNonVerbose(totalSize);

	//for (int z = 0; z <= CHANNELS; z++)
	//{
	if (verbose) {
		const hex_formatter hex3 {3};
		std::cout << '\t';		// // //
		for (int i = 0; i < CHANNELS / 2; ++i)
			std::cout << "#" << i << ": 0x" << hex3 << tracks[i].data.size() << ' ';
		std::cout << "Ptrs+Instrs: 0x" << hex3 << spaceForPointersAndInstrs << "\n\t";
		for (int i = CHANNELS / 2; i < CHANNELS; ++i)
			std::cout << "#" << i << ": 0x" << hex3 << tracks[i].data.size() << ' ';
		std::cout << "Loop:        0x" << hex3 << tracks[CHANNELS].data.size();
		std::cout << "\nSpace used by echo: 0x" << hex4 << (echoBufferSize << 11) <<
			" bytes.  Space used by samples: 0x" << hex4 << spaceUsedBySamples << " bytes.\n\n";
	}
	//}
	if (totalSize > minSize && minSize > 0) {
		std::stringstream err;		// // //
		err << "Song was larger than it could pad out by 0x" << hex4 << totalSize - minSize << " bytes.";
		warn(err.str());		// // //
	}

	std::stringstream statStrStream;

	for (int i = 0; i < CHANNELS; ++i)		// // //
		statStrStream << "CHANNEL " << ('0' + i) << " SIZE:				0x" << hex4 << tracks[i].data.size() << "\n";
	statStrStream << "LOOP DATA SIZE:				0x" << hex4 << tracks[CHANNELS].data.size() << "\n";
	statStrStream << "POINTERS AND INSTRUMENTS SIZE:		0x" << hex4 << spaceForPointersAndInstrs << "\n";
	statStrStream << "SAMPLES SIZE:				0x" << hex4 << spaceUsedBySamples << "\n";
	statStrStream << "ECHO SIZE:				0x" << hex4 << (echoBufferSize << 11) << "\n";
	statStrStream << "SONG TOTAL DATA SIZE:			0x" << hex4 << totalSize << "\n";		// // //

	if (index > highestGlobalSong)
		statStrStream << "FREE ARAM (APPROXIMATE):		0x" << hex4 << 0x10000 - (echoBufferSize << 11) - spaceUsedBySamples - totalSize - programUploadPos << "\n\n";
	else
		statStrStream << "FREE ARAM (APPROXIMATE):		UNKNOWN\n\n";

	for (int i = 0; i < CHANNELS; ++i)		// // //
		statStrStream << "CHANNEL " << ('0' + i) << " TICKS:			0x" << hex4 << tracks[i].channelLength << "\n";
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

	statStr = statStrStream.str();

	std::string fname = name;

	int extPos = fname.find_last_of('.');
	if (extPos != -1)
		fname = fname.substr(0, extPos);

	if (fname.find('/') != -1)
		fname = fname.substr(fname.find_last_of('/') + 1);
	else if (fname.find('\\') != -1)
		fname = fname.substr(fname.find_last_of('\\') + 1);
	fname = "stats/" + fname + ".txt";

	writeTextFile(fname, statStr);
}

// // //
void Music::adjustLoopPointers() {
	int normalChannelsSize = getDataSize();		// // //
	for (Track &t : tracks) {		// // //
		for (unsigned short x : t.loopLocations) {
			int temp = (t.data[x] & 0xFF) | (t.data[x + 1] << 8);
			temp += posInARAM + normalChannelsSize + spaceForPointersAndInstrs;
			t.data[x] = temp & 0xFF;
			t.data[x + 1] = temp >> 8;
		}
	}
}

// // //

void Music::parseSPCInfo() {
	if (skipSpaces(), !trimChar('{'))
		error("Could not find opening brace in SPC info command.");

	while (skipSpaces(), !trimChar('}')) {
		if (!trimChar('#'))		// // //
			error("Unexpected symbol found in SPC info command.  Expected \'#\'.");
		std::string typeName = getIdentifier();		// // //

		if (typeName != "author" && typeName != "comment" && typeName != "title" && typeName != "game" && typeName != "length")
			error("Unexpected type name found in SPC info command.  Only \"author\", \"comment\", \"title\", \"game\", and \"length\" are allowed.");

		if (skipSpaces(), !trimChar('\"'))
			error("Error while parsing parameter for SPC info command.");
		std::string parameter = getEscapedString();		// // //

		if (typeName == "author")
			author = parameter;
		else if (typeName == "comment")
			comment = parameter;
		else if (typeName == "title")
			title = parameter;
		else if (typeName == "game")
			game = parameter;
		else if (typeName == "length") {
			if (parameter == "auto")
				guessLength = true;
			else {
				guessLength = false;
				AMKd::MML::SourceFile field {parameter};		// // //
				auto param = AMKd::MML::Lexer::Time()(field);
				if (param && !field)
					seconds = *param;
				else
					error("Error parsing SPC length field.  Format must be m:ss or \"auto\".");		// // //

				if (seconds > 999)
					error("Songs longer than 16:39 are not allowed by the SPC format.");		// // //

				knowsLength = true;
			}
		}
	}

	if (author.length() > 32) {
		author = author.substr(0, 32);
		printWarning("\"Author\" field was too long.  Truncating to \"" + author + "\".");		// // //
	}
	if (game.length() > 32) {
		game = game.substr(0, 32);
		printWarning("\"Game\" field was too long.  Truncating to \"" + game + "\".");
	}
	if (comment.length() > 32) {
		comment = comment.substr(0, 32);
		printWarning("\"Comment\" field was too long.  Truncating to \"" + comment + "\".");
	}
	if (title.length() > 32) {
		title = title.substr(0, 32);
		printWarning("\"Title\" field was too long.  Truncating to \"" + title + "\".");
	}
}

void Music::addNoteLength(double ticks) {
	if (extraLoopIsNormal)
		normalLoopLength += ticks;
	else if (extraLoopIsSuper)
		superLoopLength += ticks;
	else if (baseLoopIsNormal)
		normalLoopLength += ticks;
	else if (baseLoopIsSuper)
		superLoopLength += ticks;
	else
		tracks[channel].channelLength += ticks;		// // //
}

// // //
void Music::synchronizeQ() {
	if (!inNormalLoop)		// // //
		tracks[channel].updateQ = true;		// // //
	tracks[CHANNELS].updateQ = true;
	prevNoteLength = -1;
}

int Music::divideByTempoRatio(int value, bool fractionIsError) {
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
void Music::doOctave(int oct) {
	if (oct < 1)
		error("The octave has been dropped too low.");
	if (oct > 6)
		error("The octave has been raised too high.");
	octave = oct;
}

void Music::doVolume(int vol) {
	append(AMKd::Binary::CmdType::Vol, vol);
}

void Music::doGlobalVolume(int vol) {
	append(AMKd::Binary::CmdType::VolGlobal, vol);
}

void Music::doVibrato(int delay, int rate, int depth) {
	append(AMKd::Binary::CmdType::Vibrato,
		divideByTempoRatio(delay, false), multiplyByTempoRatio(rate), depth);		// // //
}

void Music::doTremolo(int delay, int rate, int depth) {
	append(AMKd::Binary::CmdType::Tremolo,
		divideByTempoRatio(delay, false), multiplyByTempoRatio(rate), depth);		// // //
}

void Music::doTremoloOff() {
	append(AMKd::Binary::CmdType::Tremolo, 0x00, 0x00, 0x00);
}

void Music::doEnvelope(int ad, int sr) {
	append(AMKd::Binary::CmdType::Envelope, ad, sr);
}

void Music::doTempo(int speed) {
	tempo = requires(divideByTempoRatio(speed, false), 0x01, 0xFF, "Tempo has been zeroed out by #halvetempo");		// // //
	tempoDefined = true;
	append(AMKd::Binary::CmdType::Tempo, tempo);		// // //

	if (inNormalLoop || inE6Loop)		// // // Not even going to try to figure out tempo changes inside loops.  Maybe in the future.
		guessLength = false;
	else
		tempoChanges.emplace_back(tracks[channel].channelLength, tempo);		// // //
}

void Music::doSampleLoad(int index, int mult) {
	if (optimizeSampleUsage)
		usedSamples[index] = true;
	append(AMKd::Binary::CmdType::SampleLoad, index, mult);		// // //
}

void Music::doLoopEnter() {
	normalLoopLength = 0;
	if (inE6Loop) {					// We're entering a normal loop that's nested in a super loop
		baseLoopIsNormal = false;
		baseLoopIsSuper = true;
		extraLoopIsNormal = true;
		extraLoopIsSuper = false;
	}
	else {						// We're entering a normal loop that's not nested
		baseLoopIsNormal = true;
		baseLoopIsSuper = false;
		extraLoopIsNormal = false;
		extraLoopIsSuper = false;
	}
}

void Music::doLoopExit(int loopCount) {
	if (extraLoopIsNormal) {				// We're leaving a normal loop that's nested in a super loop.
		extraLoopIsNormal = false;
		extraLoopIsSuper = false;
		superLoopLength += normalLoopLength * loopCount;
	}
	else if (baseLoopIsNormal) {			// We're leaving a normal loop that's not nested.
		baseLoopIsNormal = false;
		baseLoopIsSuper = false;
		tracks[channel].channelLength += normalLoopLength * loopCount;		// // //
	}

	if (loopLabel > 0)
		loopLengths[loopLabel] = normalLoopLength;
}

void Music::doLoopRemoteCall(int loopCount) {
	addNoteLength((loopLabel ? loopLengths[loopLabel] : normalLoopLength) * loopCount);		// // //
}

void Music::doSubloopEnter() {
	if (inE6Loop)		// // //
		error("You cannot nest a subloop within another subloop.");
	inE6Loop = true;

	synchronizeQ();		// // //
	superLoopLength = 0;

	if (inNormalLoop) {		// // // We're entering a super loop that's nested in a normal loop
		baseLoopIsNormal = true;
		baseLoopIsSuper = false;
		extraLoopIsNormal = false;
		extraLoopIsSuper = true;
	}
	else {						// We're entering a super loop that's not nested
		baseLoopIsNormal = false;
		baseLoopIsSuper = true;
		extraLoopIsNormal = false;
		extraLoopIsSuper = false;
	}

	append(AMKd::Binary::CmdType::Subloop, 0x00);		// // //
}

void Music::doSubloopExit(int loopCount) {
	if (!inE6Loop)		// // //
		error("A subloop end was found outside of a subloop.");
	inE6Loop = false;

	synchronizeQ();		// // //

	if (extraLoopIsSuper) {				// We're leaving a super loop that's nested in a normal loop.
		extraLoopIsNormal = false;
		extraLoopIsSuper = false;
		normalLoopLength += superLoopLength * loopCount;
	}
	else if (baseLoopIsSuper) {			// We're leaving a super loop that's not nested.
		baseLoopIsNormal = false;
		baseLoopIsSuper = false;
		tracks[channel].channelLength += superLoopLength * loopCount;		// // //
	}

	append(AMKd::Binary::CmdType::Subloop, loopCount - 1);		// // //
}

void Music::doVolumeTable(bool louder) {
	append(AMKd::Binary::CmdType::ExtFA, AMKd::Binary::CmdOptionFA::VolTable, louder ? 0x01 : 0x00);
}
