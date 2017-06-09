#include "Music.h"
//#include "globals.h"
//#include "Sample.h"

#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <locale>		// // //

#include "globals.h"		// // //
#include "Binary/Defines.h"		// // //
#include "MML/Lexer.h"		// // //
#include "MML/Preprocessor.h"		// // //
#include "MML/Tokenizer.h"		// // //
#include "Utility/Exception.h"		// // //
#include <functional>

using Track = AMKd::Music::Track;		// // //



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

#define warn(str) ::printWarning(str, name, mml_.GetLineNumber())

#define error(str) do { \
		::printError(str, name, mml_.GetLineNumber()); \
		return; \
	} while (false)

// // //
void Music::fatalError(const std::string &str) {
	::fatalError(str, name, mml_.GetLineNumber());		// // //
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
	tracks[::channel].append(std::forward<Args>(value)...);
}

Music::Music() {
	knowsLength = false;
	totalSize = 0;
	spaceForPointersAndInstrs = 0;
	echoBufferSize = 0;
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

	for (int z = 0; z < 256; z++) {
		transposeMap[z] = 0;
		usedSamples[z] = false;
	}

	for (int z = 0; z < 19; z++)
		transposeMap[z] = tmpTrans[z];

	title = fs::path {name}.stem().string();		// // //

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
		if (!usingSMWVTable)
			doVolumeTable(true);
		append(AMKd::Binary::CmdType::ExtFA, AMKd::Binary::CmdOptionFA::EchoBuffer, echoBufferSize);
	}

	prevChannel = channel = 0;
}

void Music::compile() {
	static const auto CMDS = [] {		// // //
		AMKd::Utility::Trie<void (Music::*)()> cmd;

		cmd.Insert("!", &Music::parseExMarkDirective);
		cmd.Insert("\"", &Music::parseReplacementDirective);
		cmd.Insert("#", &Music::parseChannelDirective);
		cmd.Insert("$", &Music::parseHexCommand);
		cmd.Insert("(", &Music::parseOpenParenCommand);
		cmd.Insert("(!", &Music::parseRemoteCodeCommand);
		cmd.Insert("*", &Music::parseStarLoopCommand);
		cmd.Insert("/", &Music::parseIntroDirective);
		cmd.Insert(";", &Music::parseComment);
		cmd.Insert("<", &Music::parseLowerOctaveDirective);
		cmd.Insert(">", &Music::parseRaiseOctaveDirective);
		cmd.Insert("?", &Music::parseQMarkDirective);
		cmd.Insert("@", &Music::parseInstrumentCommand);
		cmd.Insert("@@", &Music::parseDirectInstCommand);
		cmd.Insert("a", &Music::parseNoteA);
		cmd.Insert("b", &Music::parseNoteB);
		cmd.Insert("c", &Music::parseNoteC);
		cmd.Insert("d", &Music::parseNoteD);
		cmd.Insert("e", &Music::parseNoteE);
		cmd.Insert("f", &Music::parseNoteF);
		cmd.Insert("g", &Music::parseNoteG);
		cmd.Insert("h", &Music::parseHDirective);
		cmd.Insert("l", &Music::parseLDirective);
		cmd.Insert("n", &Music::parseNCommand);
		cmd.Insert("o", &Music::parseOctaveDirective);
		cmd.Insert("p", &Music::parseVibratoCommand);
		cmd.Insert("q", &Music::parseQuantizationCommand);
		cmd.Insert("r", &Music::parseRest);
		cmd.Insert("t", &Music::parseTempoCommand);
		cmd.Insert("tuning", &Music::parseTransposeDirective);
		cmd.Insert("v", &Music::parseVolumeCommand);
		cmd.Insert("w", &Music::parseGlobalVolumeCommand);
		cmd.Insert("y", &Music::parsePanCommand);
		cmd.Insert("[", &Music::parseLoopCommand);
		cmd.Insert("[[", &Music::parseSubloopCommand);
		cmd.Insert("[[[", &Music::parseErrorLoopCommand);
		cmd.Insert("]", &Music::parseLoopEndCommand);
		cmd.Insert("]]", &Music::parseSubloopEndCommand);
		cmd.Insert("]]]", &Music::parseErrorLoopEndCommand);
		cmd.Insert("^", &Music::parseTie);
		cmd.Insert("{", &Music::parseTripletOpenDirective);
		cmd.Insert("|", &Music::parseBarDirective);
		cmd.Insert("}", &Music::parseTripletCloseDirective);

		return cmd;
	}();
	AMKd::MML::Lexer::Tokenizer tok;

	init();

	try {
		while (mml_.HasNextToken())		// // // TODO: also call this for selected lexers
			if (auto token = tok(mml_, CMDS))		// // //
				(this->*(*token))();
			else
				throw AMKd::Utility::SyntaxException {"Unexpected character \"" + *mml_.Trim(".") + "\" found."};
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
	for (size_t i = 0; i < CHANNELS; ++i)
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
//	if (songTargetProgram != Target::AMM)		// // //
//		error("Illegal use of comments. Sorry about that. Should be fixed in AddmusicK 2.");		// // //
	mml_.Trim(".*?\\n");
}

void Music::printChannelDataNonVerbose(int size) {
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
	std::cout << ", 0x" << hex4 << size << std::dec << " bytes\n";
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
	if (songTargetProgram != Target::AMK)
		mml_.Clear();
}

void Music::parseChannelDirective() {
	using namespace AMKd::MML::Lexer;		// // //
	if (auto param = GetParameters<Chan>(mml_)) {
		if (param->count() > 1)
			error("TODO: Channel multiplexing");
		for (size_t i = 0; i < CHANNELS; ++i)
			if (param.get<0>()[i]) {
				channel = i;
				resetStates();		// // //
				tracks[channel].lastDuration = 0;
				tracks[channel].usingH = false;
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
	using namespace AMKd::MML::Lexer;		// // //
	if (auto param = GetParameters<Hex<2>>(mml_))
		return writeState(&Track::q, requires(param.get<0>(), 0x01u, 0x7Fu, CMD_ILLEGAL("quantization", "q")));
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
	tracks[channel].lastDuration = 0;
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
	using namespace AMKd::MML::Lexer;
	if (auto param = GetParameters<Int>(mml_)) {
		int inst = requires(param.get<0>(), 0x00u, 0xFFu, CMD_ILLEGAL("instrument", "@"));
		return doInstrument(inst, inst < 0x13 || inst >= 30);
	}
	error(CMD_ERROR("instrument", "@"));		// // //
}

// // //
void Music::parseDirectInstCommand() {
	using namespace AMKd::MML::Lexer;
	if (auto param = GetParameters<Int>(mml_))
		return doInstrument(requires(param.get<0>(), 0x00u, 0xFFu, CMD_ILLEGAL("instrument", "@")), true);
	error(CMD_ERROR("instrument", "@"));		// // //
}

void Music::parseOpenParenCommand() {
	using namespace AMKd::MML::Lexer;		// // //
	if (auto param = GetParameters<Int>(mml_)) {
		int label = requires(param.get<0>(), 0u, 0xFFFFu, "Illegal value for loop label.");
		if (inNormalLoop)		// // //
			error("Nested loops are not allowed.");		// // //
		if (GetParameters<Sep<')', '['>>(mml_)) {		// If this is a loop definition...
			loopLabel = label + 1;
			tracks[channel].isDefiningLabelLoop = true;		// The rest of the code is handled in the respective function.
			return parseLoopCommand();
		}
		if (auto loop = GetParameters<Sep<')'>, Option<Int>>(mml_)) {		// Otherwise, if this is a loop call...
			loopLabel = label + 1;
			auto it = loopPointers.find(loopLabel);
			if (it == loopPointers.cend())
				error("Label not yet defined.");
			doLoopRemoteCall(requires(loop->value_or(1), 0x01u, 0xFFu, "Invalid loop count."), it->second);		// // //
			loopLabel = 0;
			return;
		}
		error("Error parsing label loop.");
	}
	
	int sampID;
	if (auto param = GetParameters<Sep<'@'>, Int>(mml_))
		sampID = instrToSample[requires(param.get<0>(), 0u, 29u, "Illegal instrument number for sample load command.")];
	else if (auto param2 = GetParameters<QString>(mml_)) {
		auto it = std::find(mySamples.cbegin(), mySamples.cend(), getSample(basepath / param2.get<0>(), this));		// // //
		if (it == mySamples.cend())
			error("The specified sample was not included in this song.");
		sampID = std::distance(mySamples.cbegin(), it);
	}
	else
		error("Error parsing sample load command.");

	if (auto ext = GetParameters<Comma, Byte, Sep<')'>>(mml_))
		return doSampleLoad(sampID, ext.get<0>());
	error("Error parsing sample load command.");
}

void Music::parseLabelLoopCommand() {
	using namespace AMKd::MML::Lexer;		// // //
	if (auto param2 = GetParameters<Int>(mml_)) {
		int label = requires(param2.get<0>(), 0u, 0xFFFFu, "Illegal value for loop label.");
		if (inNormalLoop)		// // //
			error("Nested loops are not allowed.");		// // //
		if (GetParameters<Sep<')', '['>>(mml_)) {		// If this is a loop definition...
			loopLabel = label + 1;
			tracks[channel].isDefiningLabelLoop = true;		// The rest of the code is handled in the respective function.
			return parseLoopCommand();
		}
		if (auto loop = GetParameters<Sep<')'>, Option<Int>>(mml_)) {		// Otherwise, if this is a loop call...
			loopLabel = label + 1;
			auto it = loopPointers.find(loopLabel);
			if (it == loopPointers.cend())
				error("Label not yet defined.");
			doLoopRemoteCall(requires(loop->value_or(1), 0x01u, 0xFFu, "Invalid loop count."), it->second);		// // //
			loopLabel = 0;
			return;
		}
	}
	error("Error parsing label loop.");
}

// // //
void Music::parseRemoteCodeCommand() {
	using namespace AMKd::MML::Lexer;

	if (targetAMKVersion < 2)
		error("Remote code commands requires #amk 2 or above.");

	if (auto param = GetParameters<Int, Comma, SInt>(mml_)) {		// // // A channel's been defined, we're parsing a remote
		if (!channelDefined)
			error("TODO: allow calling remote codes inside definitions");

		int remoteID = param.get<0>();
		int remoteOpt = param.get<1>();
		int remoteLen = 0;
		if (remoteOpt == AMKd::Binary::CmdOptionFC::Sustain || remoteOpt == AMKd::Binary::CmdOptionFC::Release) {
			if (!GetParameters<Comma>(mml_))
				error("Error parsing remote code setup. Missing the third argument.");
			if (auto len = GetParameters<Byte>(mml_))
				remoteLen = len.get<0>();
			else if (auto len2 = GetParameters<Dur>(mml_)) {		// // //
				remoteLen = getRawTicks(len2.get<0>());
				if (remoteLen > 0x100)
					error("Note length specified was too large.");		// // //
				else if (remoteLen == 0x100)
					remoteLen = 0;
			}
			else
				error("Error parsing remote code setup.");
		}

		if (!GetParameters<Sep<')'>, Sep<'['>>(mml_))
			error("Error parsing remote code setup.");

		tracks[channel].loopLocations.push_back(static_cast<uint16_t>(tracks[channel].data.size() + 1));		// // //
		if (loopPointers.find(remoteID) == loopPointers.cend())		// // //
			loopPointers.insert({remoteID, static_cast<uint16_t>(-1)});
		append(AMKd::Binary::CmdType::Callback, loopPointers[remoteID] & 0xFF, loopPointers[remoteID] >> 8, remoteOpt, remoteLen);
		return;
	}

	if (auto param = GetParameters<Int, Sep<')'>, Sep<'['>>(mml_)) {		// // // We're outside of a channel, this is a remote call definition.
		if (channelDefined)
			error("TODO: allow inline remote code definitions");
		loopLabel = param.get<0>();
		inRemoteDefinition = true;
		return parseLoopCommand();
	}

	error("Error parsing remote code command.");
}

void Music::parseLoopCommand() {
	prevLoop = static_cast<uint16_t>(tracks[CHANNELS].data.size());		// // //
	if (loopLabel > 0) {
		if (loopPointers.find(loopLabel) != loopPointers.cend())		// // //
			error("Label redefinition.");
		loopPointers[loopLabel] = prevLoop;
	}

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
	using namespace AMKd::MML::Lexer;		// // //
	auto param = GetParameters<Option<Int>>(mml_);
	if (param && inRemoteDefinition)
		error("Remote code definitions cannot repeat.");		// // //
	doLoopExit(requires(param->value_or(1), 0x01u, 0xFFu, "Invalid loop count."));
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
	using namespace AMKd::MML::Lexer;		// // //
	auto param = GetParameters<Option<Int>>(mml_);
	if (inNormalLoop)		// // //
		error("Nested loops are not allowed.");		// // //
	return doLoopRemoteCall(requires(param->value_or(1), 0x01u, 0xFFu, CMD_ILLEGAL("star loop", "*")), prevLoop);
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
	if (std::exchange(tracks[channel].inTriplet, true))		// // //
		error("Triplet on directive found within a triplet block.");
}

void Music::parseTripletCloseDirective() {
	if (!std::exchange(tracks[channel].inTriplet, false))		// // //
		error("Triplet off directive found outside of a triplet block.");
}

void Music::parseRaiseOctaveDirective() {
	return doRaiseOctave();		// // //
}

void Music::parseLowerOctaveDirective() {
	return doLowerOctave();		// // //
}

void Music::parseHFDInstrumentHack(int addr, int bytes) {
	using namespace AMKd::MML::Lexer;		// // //

	int byteNum = 0;
	for (; bytes >= 0; --bytes) {
		if (auto param = GetParameters<Byte>(mml_)) {
			instrumentData.push_back(param.get<0>());		// // //
			++addr;
			++byteNum;
			if (byteNum == 1) {
				if (optimizeSampleUsage)
					usedSamples[param.get<0>()] = true;
			}
			else if (byteNum == 5) {
				instrumentData.push_back(0);	// On the 5th byte, we need to add a 0 as the new sub-multiplier.
				byteNum = 0;
			}
			continue;
		}
		error("Unknown HFD hex command.");
	}
}

void Music::parseHFDHex() {
	using namespace AMKd::MML::Lexer;		// // //
	auto kind = GetParameters<Byte>(mml_);
	if (!kind)
		error("Unknown HFD hex command.");
	auto i = kind.get<0>();

	if (convert) {
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
	auto pos = static_cast<uint16_t>(tracks[channel].data.size() + 1);
	tracks[channel].remoteGainInfo.emplace_back(pos, std::move(cmd));		// // //
	append(AMKd::Binary::CmdType::Callback, 0x00, 0x00, cmdtype, param);		// // //
}

// // //

void Music::parseHexCommand() {
	auto hex = AMKd::MML::Lexer::Hex<2>()(mml_);		// // //
	if (!hex)
		error("Error parsing hex command.");
	int i = *hex;

	using namespace AMKd::MML::Lexer;
	const auto getval = [&] {		// // // TODO: remove
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
		append(i);
		getval();		// // // no more parse hacks
		append(i);
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
			for (unsigned j = 0; j < count; ++j) {
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
	int note = getPitch(offset);
	if (tracks[channel].instrument >= 21 && tracks[channel].instrument < 30) {		// // //
		note = 0xD0 + (tracks[channel].instrument - 21);
		if (!(channel == 6 || channel == 7 || (inNormalLoop && (prevChannel == 6 || prevChannel == 7))))	// If this is not a SFX channel,
			tracks[channel].instrument = 0xFF;										// Then don't force the drum pitch on every note.
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
		tracks[channel].usingH = true;		// // //
		tracks[channel].h = static_cast<int8_t>(
			requires(param.get<0>(), -128, 127, DIR_ILLEGAL("transpose (\"h\")")));
		return;
	}
	error(DIR_ERROR("transpose (\"h\")"));
}

void Music::parseNCommand() {
	using namespace AMKd::MML::Lexer;		// // //
	if (auto param = GetParameters<Hex<2>>(mml_))
		return append(AMKd::Binary::CmdType::Noise, requires(param.get<0>(), 0x00u, 0x1Fu, CMD_ILLEGAL("noise", "n")));		// // //
	error(CMD_ERROR("noise", "n"));
}

// // //
void Music::parseBarDirective() {
	// hexLeft = 0;
}

void Music::parseOptionDirective() {
	if (targetAMKVersion == 1)
		error("Unknown command.");
	if (channelDefined)
		error("#option directives must be used before any and all channels.");		// // //

	static const auto CMDS = [] {		// // //
		AMKd::Utility::Trie<void (Music::*)()> cmd;

		cmd.Insert("smwvtable", &Music::parseSMWVTable);
		cmd.Insert("nspcvtable", &Music::parseNSPCVTable);
		cmd.Insert("tempoimmunity", &Music::parseTempoImmunity);
		cmd.Insert("noloop", &Music::parseNoLoop);
		cmd.Insert("dividetempo", &Music::parseDivideTempo);

		return cmd;
	}();

	using namespace AMKd::MML::Lexer;		// // //
	if (auto param = GetParameters<Ident>(mml_)) {
		downcase(param.get<0>());
		std::string_view sv {param.get<0>()};
		if (auto func = CMDS.SearchValue(sv))
			return (this->*(*func))();
	}
	error("Unknown option type for '#option' directive.");		// // //
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
	append(AMKd::Binary::CmdType::ExtF4, AMKd::Binary::CmdOptionF4::TempoImmunity);		// // //
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
		error("Argument for #option dividetempo cannot be 0.");		// // //
	}
	error("Missing integer argument for #option dividetempo.");		// // //
}

void Music::parseHalveTempo() {
	if (channelDefined)
		error("#halvetempo must be used before any and all channels.");		// // //
	tempoRatio = requires(tempoRatio * 2, 1, 256, "#halvetempo has been used too many times...what are you even doing?");
}

void Music::parseSpecialDirective() {
	static const auto CMDS = [] {		// // //
		AMKd::Utility::Trie<void (Music::*)()> cmd;

		cmd.Insert("instruments", &Music::parseInstrumentDefinitions);
		cmd.Insert("samples", &Music::parseSampleDefinitions);
		cmd.Insert("pad", &Music::parsePadDefinition);
		cmd.Insert("spc", &Music::parseSPCInfo);
		cmd.Insert("louder", &Music::parseLouderCommand);
		cmd.Insert("tempoimmunity", &Music::parseTempoImmunity);
		cmd.Insert("path", &Music::parsePath);
		cmd.Insert("halvetempo", &Music::parseHalveTempo);
		cmd.Insert("option", &Music::parseOptionDirective);

		return cmd;
	}();

	using namespace AMKd::MML::Lexer;		// // //
	if (auto param = GetParameters<Ident>(mml_)) {
		downcase(param.get<0>());
		std::string_view sv {param.get<0>()};
		if (auto func = CMDS.SearchValue(sv))
			return (this->*(*func))();
	}
	error(DIR_ERROR("'#'"));
}

void Music::parseReplacementDirective() {
	using namespace AMKd::MML::Lexer;		// // //
	mml_.Unput();
	auto param = GetParameters<QString>(mml_);
	if (!param)
		fatalError("Unexpected end of replacement directive.");
	std::string s = param.get<0>();
	size_t i = s.find('=');
	if (i == std::string::npos)
		error("Error parsing replacement directive; could not find '='");		// // //

	std::string findStr = s.substr(0, i);
	std::string replStr = s.substr(i + 1);

	// // //
	while (!findStr.empty() && isspace(findStr.back()))
		findStr.pop_back();
	if (findStr.empty())
		error("Error parsing replacement directive; string to find was of zero length.");

	while (!replStr.empty() && isspace(replStr.front()))
		replStr.erase(0, 1);

	mml_.AddMacro(findStr, replStr);		// // //
}

void Music::parseInstrumentDefinitions() {
	using namespace AMKd::MML::Lexer;		// // //

	const auto pushInst = [this] (int inst) {
		auto param = GetParameters<Multi<Byte>>(mml_);
		if (!param || param->size() != 5)
			fatalError("Error parsing instrument definition; there must be 5 bytes following the sample.");
		instrumentData.push_back(static_cast<uint8_t>(inst));		// // //
		for (const auto &x : param.get<0>())
			instrumentData.push_back(std::get<0>(x));
		if (optimizeSampleUsage)
			usedSamples[inst] = true;
	};
	
	if (GetParameters<Sep<'{'>>(mml_)) {
		while (true) {
			if (auto brr = GetParameters<QString>(mml_)) {		// // //
				const std::string &brrName = brr.get<0>();
				if (brrName.empty())
					fatalError("Error parsing sample portion of the instrument definition.");
				auto it = std::find(mySamples.cbegin(), mySamples.cend(), getSample(basepath / brrName, this));		// // //
				if (it == mySamples.cend())
					fatalError("The specified sample was not included in this song.");		// // //
				pushInst(std::distance(mySamples.cbegin(), it));
			}
			else if (auto noi = GetParameters<Sep<'n'>, Hex<2>>(mml_))
				pushInst(0x80 | requires(noi.get<0>(), 0x00u, 0x1Fu,
										 "Invalid noise pitch.  The value must be a hex value from 0 - 1F."));
			else if (auto inst = GetParameters<Sep<'@'>, Int>(mml_))
				pushInst(instrToSample[requires(inst.get<0>(), 0u, 29u,
												"Cannot use a custom instrument's sample as a base for another custom instrument.")]);
			else
				break;
		}
		if (GetParameters<Sep<'}'>>(mml_))
			return;
	}
	fatalError("Could not find opening curly brace in instrument definition.");
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
					fatalError("The filename for the sample was invalid.  Only \".brr\" and \".bnk\" are allowed.");		// // //
			}
			else if (auto group = GetParameters<Sep<'#'>, Ident>(mml_))
				addSampleGroup(group.get<0>(), this);		// // //
			else
				break;
		}
		if (GetParameters<Sep<'}'>>(mml_))
			return;
		fatalError("Unexpected end of sample group definition.");
	}
	fatalError("Could not find opening brace in sample group definition.");
}

void Music::parsePadDefinition() {
	using namespace AMKd::MML::Lexer;
	if (GetParameters<Sep<'{'>>(mml_)) {		// // //
		if (auto param = HexInt()(mml_)) {
			minSize = *param;
			return;
		}
	}

	error(DIR_ERROR("padding"));		// // //
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
		fatalError("Unexpected symbol found in path command. Expected a quoted string.");
}

int Music::getPitch(int i) {
	i += (tracks[channel].o.Get() - 1) * 12 + 0x80;		// // //
	using namespace AMKd::MML::Lexer;
	i += GetParameters<Acc>(mml_)->offset;

	if (tracks[channel].usingH)		// // //
		i += tracks[channel].h;
	else if (!tracks[channel].ignoreTuning)		// // // More AM4 tuning stuff
		i -= transposeMap[tracks[channel].instrument];
	
	return requires(i, 0x80, static_cast<int>(AMKd::Binary::CmdType::Tie) - 1, "Note's pitch is out of range.");
}

// // //
int Music::getRawTicks(const AMKd::MML::Duration &dur) const {
	return checkTickFraction(dur.GetTicks() / tempoRatio);
}

int Music::getFullTicks(const AMKd::MML::Duration &dur) const {
	return checkTickFraction(dur.GetTicks(tracks[channel].l.Get()) / tempoRatio * (tracks[channel].inTriplet ? 2. / 3. : 1.));
}

int Music::getLastTicks(const AMKd::MML::Duration &dur) const {
	return checkTickFraction(dur.GetLastTicks(tracks[channel].l.Get()) / tempoRatio * (tracks[channel].inTriplet ? 2. / 3. : 1.));
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
	if (errorCount)
		fatalError("There were errors when compiling the music file.  Compilation aborted.  Your ROM has not been modified.");

	if (std::all_of(tracks, tracks + CHANNELS, [] (const Track &t) { return t.data.empty(); }))		// // //
		error("This song contained no musical data!");

	if (targetAMKVersion == 1)			// Handle more conversion of the old $FC command to remote call.
		for (auto &track : tracks) for (const auto &x : track.remoteGainInfo) {		// // //
			uint16_t dataIndex = x.first;
			track.loopLocations.push_back(dataIndex);
			assign_short(track.data.begin() + dataIndex, tracks[CHANNELS].data.size());
			tracks[CHANNELS].data.insert(tracks[CHANNELS].data.end(), x.second.cbegin(), x.second.cend());
		}

	for (size_t z = 0; z < CHANNELS; z++)		// // //
		if (!tracks[z].data.empty())
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
				mySamples[z] = static_cast<uint16_t>(emptySampleIndex);		// // //
	}

	int binpos = 0;		// // //
	for (size_t i = 0; i < CHANNELS; ++i) {
		if (!tracks[i].data.empty())
			tracks[i].phrasePointers[0] = static_cast<uint16_t>(binpos);
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

	assign_short(allPointersAndInstrs.begin(), add + instrumentData.size());		// // //

	/*
	FFFF -> 00, 00
	FFFE -> FF, 00
	FFFD -> 0002 + ARAMPos
	FFFC -> ARAMPos
	*/
	if (doesntLoop)		// // //
		assign_short(allPointersAndInstrs.begin() + add - 2, 0xFFFF);
	else {
		assign_short(allPointersAndInstrs.begin() + add - 4, 0xFFFE);
		assign_short(allPointersAndInstrs.begin() + add - 2, hasIntro ? 0xFFFD : 0xFFFC);
	}
	if (hasIntro)
		assign_short(allPointersAndInstrs.begin() + 2, add + instrumentData.size() + 16);		// // //

	add += instrumentData.size();
	for (size_t i = 0; i < CHANNELS; ++i) {		// // //
		auto adr = tracks[i].data.empty() ? 0xFFFB : (tracks[i].phrasePointers[0] + spaceForPointersAndInstrs);
		assign_short(allPointersAndInstrs.begin() + i * 2 + add, adr);		// // //
	}

	if (hasIntro) {
		for (size_t i = 0; i < CHANNELS; ++i) {		// // //
			auto adr = tracks[i].data.empty() ? 0xFFFB : (tracks[i].phrasePointers[1] + spaceForPointersAndInstrs);
			assign_short(allPointersAndInstrs.begin() + i * 2 + 16 + add, adr);		// // //
		}
	}

	totalSize = spaceForPointersAndInstrs + tracks[CHANNELS].data.size() + getDataSize();		// // //

	//if (tempo == -1) tempo = 0x36;
	unsigned int totalLength;
	mainLength = static_cast<unsigned>(-1);		// // //
	for (size_t i = 0; i < CHANNELS; i++)
		if (tracks[i].channelLength != 0)		// // //
			mainLength = std::min(mainLength, (unsigned int)tracks[i].channelLength);
	if (static_cast<int>(mainLength) == -1)
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
		for (size_t i = 0; i < CHANNELS / 2; ++i)
			std::cout << "#" << i << ": 0x" << hex3 << tracks[i].data.size() << ' ';
		std::cout << "Ptrs+Instrs: 0x" << hex3 << spaceForPointersAndInstrs << "\n\t";
		for (size_t i = CHANNELS / 2; i < CHANNELS; ++i)
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

	for (size_t i = 0; i < CHANNELS; ++i)		// // //
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

	for (size_t i = 0; i < CHANNELS; ++i)		// // //
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

	auto fname = "stats" / fs::path {name}.stem();		// // //
	fname.replace_extension(".txt");
	writeTextFile(fname, statStr);
}

// // //
void Music::adjustLoopPointers() {
	int normalChannelsSize = getDataSize();		// // //
	for (Track &t : tracks) {		// // //
		for (unsigned short x : t.loopLocations) {
			int temp = (t.data[x] & 0xFF) | (t.data[x + 1] << 8);
			assign_short(t.data.begin() + x, temp + posInARAM + normalChannelsSize + spaceForPointersAndInstrs);		// // //
		}
	}
}

// // //

void Music::parseSPCInfo() {
	using namespace AMKd::MML::Lexer;		// // //

	if (!GetParameters<Sep<'{'>>(mml_))
		fatalError("Could not find opening brace in SPC info command.");

	while (auto item = GetParameters<Sep<'#'>, Ident, QString>(mml_)) {
		const std::string &metaName = item.get<0>();
		std::string metaParam = item.get<1>();

		if (metaName == "length") {
			guessLength = (metaParam == "auto");
			if (!guessLength) {
				AMKd::MML::SourceFile field {metaParam};		// // //
				auto param = AMKd::MML::Lexer::Time()(field);
				if (param && !field)
					seconds = requires(*param, 0u, 999u, "Songs longer than 16:39 are not allowed by the SPC format.");		// // //
				else
					error("Error parsing SPC length field.  Format must be m:ss or \"auto\".");		// // //
				knowsLength = true;
			}
			continue;
		}

		if (metaParam.size() > 32) {
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
		else
			error("Unexpected type name found in SPC info command.  Only \"author\", \"comment\", \"title\", \"game\", and \"length\" are allowed.");
	}

	if (!GetParameters<Sep<'}'>>(mml_))
		fatalError("Unexpected end of SPC info command.");
}

void Music::addNoteLength(double ticks) {
	if (loopState2 != LoopType::none)		// // //
		(loopState2 == LoopType::sub ? superLoopLength : normalLoopLength) += ticks;
	else if (loopState1 != LoopType::none)
		(loopState1 == LoopType::sub ? superLoopLength : normalLoopLength) += ticks;
	else
		tracks[channel].channelLength += ticks;		// // //
}

// // //
void Music::writeState(AMKd::Music::TrackState (AMKd::Music::Track::*state), int val) {
	tracks[inNormalLoop ? prevChannel : channel].*state = val;		// // //
	tracks[CHANNELS].*state = val;
}

void Music::resetStates() {
	tracks[CHANNELS].q = tracks[channel].q;
	tracks[CHANNELS].o = tracks[channel].o;
	tracks[CHANNELS].l = tracks[channel].l;
}

void Music::synchronizeStates() {
	if (!inNormalLoop) {		// // //
		tracks[channel].q.Update();
		tracks[channel].o.Update();
		tracks[channel].l.Update();
	}
	tracks[CHANNELS].q.Update();
	tracks[CHANNELS].o.Update();
	tracks[CHANNELS].l.Update();
	tracks[channel].lastDuration = 0;
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
void Music::doNote(int note, int fullTicks, int bendTicks, bool nextPorta) {
	if (inRemoteDefinition)
		error("Remote definitions cannot contain note data!");
	if (songTargetProgram == Target::AMK && channelDefined == false && inRemoteDefinition == false)
		error("Note data must be inside a channel!");

	const int CHUNK_MAX_TICKS = 0x7F; // divideByTempoRatio(0x60, true);
	int flatTicks = fullTicks - bendTicks;
	if (flatTicks < 0)
		fatalError("Something happened");
	addNoteLength(flatTicks + bendTicks);
	if (bendTicks > CHUNK_MAX_TICKS) {
		warn("The pitch bend here will not fully span the note's duration from its last tie.");
		flatTicks += bendTicks % CHUNK_MAX_TICKS;
		bendTicks -= bendTicks % CHUNK_MAX_TICKS;
	}

	const auto doSingleNote = [this] (int note, int len) {		// // //
		if (tracks[channel].q.NeedsUpdate()) {
			append(tracks[channel].lastDuration = static_cast<uint8_t>(len));
			append(tracks[channel].q.Get());
		}
		if (tracks[channel].lastDuration != len)
			append(tracks[channel].lastDuration = static_cast<uint8_t>(len));
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

	if (tracks[channel].inPitchSlide)		// // //
		append(AMKd::Binary::CmdType::Portamento, 0x00, tracks[channel].lastDuration, note);		// // //
	tracks[channel].inPitchSlide = nextPorta;
	flushNote(note, flatTicks);
	flushNote(note, bendTicks);
}

void Music::doOctave(int oct) {
	return writeState(&Track::o, requires(oct, 1, 6, DIR_ILLEGAL("octave (\"o\")")));
}

void Music::doRaiseOctave() {
	int oct = tracks[channel].o.Get();		// // //
	if (oct >= 6)
		error("The octave has been raised too high.");
	return writeState(&Track::o, ++oct);
}

void Music::doLowerOctave() {
	int oct = tracks[channel].o.Get();		// // //
	if (oct <= 1)
		error("The octave has been dropped too low.");
	return writeState(&Track::o, --oct);
}

void Music::doVolume(int vol) {
	append(AMKd::Binary::CmdType::Vol, vol);
}

void Music::doGlobalVolume(int vol) {
	append(AMKd::Binary::CmdType::VolGlobal, vol);
}

void Music::doInstrument(int inst, bool add) {
	if (add) {
		if (convert && inst >= 0x13 && inst < 30)	// Change it to an HFD custom instrument.
			inst = inst - 0x13 + 30;
		if (optimizeSampleUsage) {
			if (inst < 30)
				usedSamples[instrToSample[inst]] = true;
			else if ((inst - 30) * 6 < static_cast<int>(instrumentData.size()))		// // //
				usedSamples[instrumentData[(inst - 30) * 6]] = true;
			else
				error("This custom instrument has not been defined yet.");		// // //
		}
		if (songTargetProgram == Target::AM4)		// // //
			tracks[channel].ignoreTuning = false;
		append(AMKd::Binary::CmdType::Inst, inst);		// // //
	}
	if (optimizeSampleUsage && inst < 30)
		usedSamples[instrToSample[inst]] = true;

	tracks[channel].instrument = inst;		// // //
	/*
	hTranspose = 0;
	usingHTranspose = false;
	if (htranspose[inst])
		transposeMap[tracks[channe].instrument] = tmpTrans[tracks[channe].instrument];
	*/
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
	append(AMKd::Binary::CmdType::Tempo, tempo);		// // //

	if (inNormalLoop || inSubLoop)		// // // Not even going to try to figure out tempo changes inside loops.  Maybe in the future.
		guessLength = false;
	else
		tempoChanges.emplace_back(tracks[channel].channelLength, tempo);		// // //
}

void Music::doSampleLoad(int id, int mult) {
	if (optimizeSampleUsage)
		usedSamples[id] = true;
	append(AMKd::Binary::CmdType::SampleLoad, id, mult);		// // //
}

void Music::doLoopEnter() {
	synchronizeStates();		// // //
	if (std::exchange(inNormalLoop, true))		// // //
		error("You cannot nest standard [ ] loops.");

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
	channel = CHANNELS;					// So we have to back up the current channel.
	tracks[CHANNELS].instrument = tracks[prevChannel].instrument;		// // //
	if (songTargetProgram == Target::AM4)
		tracks[CHANNELS].ignoreTuning = tracks[prevChannel].ignoreTuning; // More AM4 tuning stuff.  Related to the line above it.
}

void Music::doLoopExit(int loopCount) {
	append(AMKd::Binary::CmdType::End);		// // //
	synchronizeStates();		// // //
	channel = prevChannel;

	if (!std::exchange(inNormalLoop, false))		// // //
		error("Loop end found outside of a loop.");

	if (loopState2 == LoopType::normal) {				// We're leaving a normal loop that's nested in a subloop.
		loopState2 = LoopType::none;
		superLoopLength += normalLoopLength * loopCount;
	}
	else if (loopState1 == LoopType::normal) {			// We're leaving a normal loop that's not nested.
		loopState1 = LoopType::none;
		tracks[channel].channelLength += normalLoopLength * loopCount;		// // //
	}

	if (loopLabel > 0)
		loopLengths[loopLabel] = normalLoopLength;

	if (!inRemoteDefinition) {
		tracks[channel].loopLocations.push_back(static_cast<uint16_t>(tracks[channel].data.size() + 1));		// // //
		append(AMKd::Binary::CmdType::Loop, prevLoop & 0xFF, prevLoop >> 8, loopCount);
	}
	inRemoteDefinition = false;
	loopLabel = 0;
	tracks[channel].isDefiningLabelLoop = false;		// // //
}

void Music::doLoopRemoteCall(int loopCount, uint16_t loopAdr) {
	synchronizeStates();		// // //
	addNoteLength((loopLabel ? loopLengths[loopLabel] : normalLoopLength) * loopCount);		// // //
	tracks[channel].loopLocations.push_back(static_cast<uint16_t>(tracks[channel].data.size() + 1));		// // //
	append(AMKd::Binary::CmdType::Loop, loopAdr & 0xFF, loopAdr >> 8, loopCount);
}

void Music::doSubloopEnter() {
	synchronizeStates();		// // //
	if (std::exchange(inSubLoop, true))		// // //
		error("You cannot nest a subloop within another subloop.");

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
		error("A subloop end was found outside of a subloop.");

	if (loopState2 == LoopType::sub) {				// We're leaving a subloop that's nested in a normal loop.
		loopState2 = LoopType::none;
		normalLoopLength += superLoopLength * loopCount;
	}
	else if (loopState1 == LoopType::sub) {			// We're leaving a subloop that's not nested.
		loopState1 = LoopType::none;
		tracks[channel].channelLength += superLoopLength * loopCount;		// // //
	}

	append(AMKd::Binary::CmdType::Subloop, loopCount - 1);		// // //
}

void Music::doVolumeTable(bool louder) {
	if (louder)
		append(AMKd::Binary::CmdType::ExtF4, AMKd::Binary::CmdOptionF4::VelocityTable);
	else
		append(AMKd::Binary::CmdType::ExtFA, AMKd::Binary::CmdOptionFA::VolTable, /*louder ? 0x01 :*/ 0x00);
}
