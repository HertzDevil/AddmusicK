#include "Music.h"
//#include "globals.h"
//#include "Sample.h"

#include <iostream>
#include <iomanip>
#include <cstring>
#include <cmath>
#include <sstream>
#include <algorithm>
#include <locale>		// // //

#include "globals.h"		// // //
#include "Utility/Trie.h"		// // //
#include "Binary/Defines.h"		// // //
#include <functional>

//#include "Preprocessor.h"


// // //

#define error(str) do {					\
	printError(str, false, name, line);	\
	return; 							\
} while (false)

static int line, channel, prevChannel, octave, prevNoteLength, defaultNoteLength;
static bool inDefineBlock;
static bool inNormalLoop;		// // //

static unsigned int prevLoop;
static bool doesntLoop;
static bool triplet;
static bool inPitchSlide;

// // //
static enum class TargetType {
	AMK, AM4, AMM, Unknown = AMK,
} songTargetProgram;
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
static int hexLeft = 0;
static int currentHex = 0;

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

//static bool songVersionIdentified;
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
	(tracks[channel].data.push_back(static_cast<uint8_t>(value)), ...);
#else
	SWALLOW(tracks[channel].data.push_back(static_cast<uint8_t>(value)));
#endif
}

// // //
bool Music::trimChar(char c) {
	if (peek() == c) { // not safe for all inputs
		skipChars(1);
		return true;
	}
	return false;
}

// // //
bool Music::trimDirective(std::string_view str) {
	size_t l = str.size();
	auto &text_ = text;
	if (text_.size() < l)
		return false;
	auto prefix = text_.substr(0, l);

	// case-insensitive
	std::use_facet<std::ctype<char>>(std::locale("")).tolower(&prefix[0], &prefix[0] + l);

	if (prefix == str) { // no need to check that next character is a space
		skipChars(l);
		return true;
	}

	return false;
}

// // //
void Music::skipChars(size_t count) {
	auto &text_ = text;
	text_.erase(0, count);
}

// // //
void Music::skipSpaces() {
	auto &text_ = text;
	while (!text_.empty() && isspace(text_.front())) {
		if (text_.front() == '\n')
			++line;
		skipChars(1);
	}
}

// // //
[[noreturn]] void Music::fatalError(const std::string &str) {
	printError(str, true, name, line);
}

// // //
bool Music::doReplacement(std::string &str, std::size_t whence) {
	AMKd::Utility::Trie<bool> prefix;

	while (true) {
		auto it = std::find_if(replacements.cbegin(), replacements.cend(), [&] (const auto &x) {
			const std::string &rhs = x.first;
			return std::string_view(str.c_str() + whence, rhs.length()) == rhs;
		});
		if (it == replacements.cend())
			break;
		str.replace(str.begin() + whence, str.begin() + whence + it->first.length(),
					it->second.begin(), it->second.end());
		if (prefix.SearchIndex(str.c_str() + whence) != std::string_view::npos)
			return false;
		prefix.Insert(it->first, true);
	}

	return true;
}

Music::Music() {
	knowsLength = false;
	playOnce = false;
	hasIntro = false;
	totalSize = 0;
	spaceForPointersAndInstrs = 0;
	exists = false;
	echoBufferSize = 0;
	noteParamaterByteCount = 0;

	//if (validateHex)		// Allow space for the buffer reservation header.
	//	data[0].resize(3);
}

void Music::init() {
	basepath = "./";
	prevChannel = 0;
	openTextFile((std::string)"music/" + name, text);
	manualNoteWarning = true;
	tempoDefined = false;
	//am4silence = 0;
	//songVersionIdentified = false;
	// // //
	hasYoshiDrums = false;
	//onlyHadOneTempo = true;
	tempo = 0x36;
	guessLength = true;

	channelDefined = false;
	tempoRatio = 1;
	nextHexIsArpeggioNoteLength = false;

	// // //
	//remoteDefinitionArg = 0;
	inRemoteDefinition = false;
	inNormalLoop = false;		// // //

	superLoopLength = normalLoopLength = 0;

	baseLoopIsNormal = baseLoopIsSuper = extraLoopIsNormal = extraLoopIsSuper = false;
	// // //

	inE6Loop = false;
	seconds = 0;

	songTargetProgram = TargetType::AMK;		// // //
	hTranspose = 0;
	usingHTranspose = false;
	line = 1;
	channel = 0;
	octave = 4;
	prevNoteLength = -1;
	defaultNoteLength = 8;

	prevLoop = -1;
	// // //

	inDefineBlock = false;

	for (auto &t : tracks)		// // //
		t.ignoreTuning = (songTargetProgram == TargetType::AM4); // AM4 fix for tuning[] related stuff.

	hasIntro = false;
	doesntLoop = false;
	triplet = false;
	inPitchSlide = false;

	loopLabel = 0;
	currentHex = 0;
	hexLeft = 0;

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

	const auto stat = preprocess(text, name);		// // //
	text = stat.result;		// // //
	if (!stat.title.empty())
		title = stat.title;

	//Preprocessor preprocessor;

	//preprocessor.run(text, name);

	if (stat.version == -1) {
		songTargetProgram = TargetType::AM4;		// // //
		//songVersionIdentified = true;
		targetAMKVersion = 0;
	}

	else if (stat.version == -2) {
		songTargetProgram = TargetType::AMM;		// // //
		//songVersionIdentified = true;
		targetAMKVersion = 0;
	}
	else if (stat.version == 0)
		error("Song did not specify target program with #amk, #am4, or #amm.");
	else {						// Just assume it's AMK for now.
		targetAMKVersion = stat.version;

		if (targetAMKVersion > PARSER_VERSION)
			error("This song was made for a newer version of AddmusicK.  You must update to use\nthis song.");

		/*			targetAMKVersion = 0;
		if (backup.find('\r') != -1)
		backup = backup.insert(backup.length(), "\r\n\r\n#amk=1\r\n");		// Automatically assume that this is a song written for AMK.
		else
		backup = backup.insert(backup.length(), "\n\n#amk=1\n");
		writeTextFile((std::string)"music/" + name, backup);*/
	}

	usingSMWVTable = (targetAMKVersion < 2);		// // //
	
	// // // We can't just insert this at the end due to looping complications and such.
	if (validateHex && index > highestGlobalSong && stat.firstChannel != CHANNELS) {
		auto &data = tracks[stat.firstChannel].data;
		if (targetAMKVersion > 1) {
			std::vector<uint8_t> header {AMKd::Binary::CmdType::ExtFA, AMKd::Binary::CmdOptionFA::VolTable, 0x01};
			data.insert(data.cbegin(), header.cbegin(), header.cend());
		}
		std::vector<uint8_t> header {AMKd::Binary::CmdType::ExtFA, AMKd::Binary::CmdOptionFA::EchoBuffer, static_cast<uint8_t>(echoBufferSize)};
		data.insert(data.cbegin(), header.cbegin(), header.cend());
	}
}

void Music::compile() {
	init();

	while (hasNextToken()) {		// // //
		char ch = ::tolower(peek());		// // //
		skipChars(1);

		if (hexLeft != 0 && ch != '$') {
			if (songTargetProgram == TargetType::AM4 && currentHex == AMKd::Binary::CmdType::Subloop) {		// // //
				// tremolo off
				tracks[channel].data.pop_back();
				append(AMKd::Binary::CmdType::Tremolo, 0x00, 0x00, 0x00);
				hexLeft = 0;
			}
			else
				error("Unknown hex command.");
		}
		switch (ch) {
		case '?': parseQMarkDirective();		break;
//		case '!': parseExMarkDirective();		break;
		case '#': parseChannelDirective();		break;
		case 'l': parseLDirective();			break;
		case 'w': parseGlobalVolumeCommand();	break;
		case 'v': parseVolumeCommand();			break;
		case 'q': parseQuantizationCommand();	break;
		case 'y': parsePanCommand();			break;
		case '/': parseIntroDirective();		break;
		case 't': parseT();						break;
		case 'o': parseOctaveDirective();		break;
		case '@': parseInstrumentCommand();		break;
		case '(': parseOpenParenCommand();		break;
		case '[': parseLoopCommand();			break;
		case ']': parseLoopEndCommand();		break;
		case '*': parseStarLoopCommand();		break;
		case 'p': parseVibratoCommand();		break;
		case '{': parseTripletOpenDirective();	break;
		case '}': parseTripletCloseDirective();	break;
		case '>': parseRaiseOctaveDirective();	break;
		case '<': parseLowerOctaveDirective();	break;
		case '&': parsePitchSlideCommand();		break;
		case '$': parseHexCommand();			break;
		case 'h': parseHDirective();			break;
		case 'n': parseNCommand();				break;
		case '"': parseReplacementDirective();	break;
		case '|':								break;		// // // no-op
		case 'c': case 'd': case 'e': case 'f': case 'g': case 'a': case 'b': case 'r': case '^':
			parseNote(ch);						break;
		case ';': parseComment();				break;		// Needed for comments in quotes
		default:
			std::cerr << "File " << name << ", line " << line
				<< ": Unexpected character \"" << ch << "\" found." << std::endl;
			break;
		}
	}

	pointersFirstPass();
}

void Music::parseComment() {
	if (songTargetProgram == TargetType::AMM) {		// // //
		while (peek() != std::string::npos && peek() != '\n')		// // //
			skipChars(1);
		++line;
	}
	else
		error("Illegal use of comments. Sorry about that. Should be fixed in AddmusicK 2.");		// // //
}

void Music::printChannelDataNonVerbose(int totalSize) {
	int n = 60 - printf("%s: ", name.c_str());
	for (int i = 0; i < n; i++)
		putchar('.');
	putchar(' ');

	if (knowsLength) {
		int s = (unsigned int)std::floor((mainLength + introLength) / (2.0 * tempo) + 0.5);
		printf("%d:%02d, 0x%04X bytes\n",
			(int)(std::floor((introSeconds + mainSeconds) / 60) + 0.5),
			(int)(std::floor(introSeconds + mainSeconds) + 0.5) % 60, totalSize);
	}
	else
		printf("?:??, 0x%04X bytes\n", totalSize);
}

void Music::parseQMarkDirective() {
	int i = getInt();		// // //
	switch (i) {
	case -1:
	case 0: doesntLoop = true; break;
	case 1: tracks[channel].noMusic[0] = true; break;		// // //
	case 2: tracks[channel].noMusic[1] = true; break;
	}
}

void Music::parseExMarkDirective() {
	text.clear();		// // //
}

void Music::parseChannelDirective() {
	if (isalpha(peek())) {		// // //
		parseSpecialDirective();
		return;
	}

	int i = getInt();		// // //
	if (i == -1)
		error("Error parsing channel directive.");		// // //
	if (i < 0 || i >= CHANNELS)
		error("Illegal value for channel directive.");		// // //

	channel = i;
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
}

void Music::parseLDirective() {
	int i = getInt();		// // //
	if (i == -1)
		error("Error parsing \"l\" directive.");		// // //
	if (i < 1 || i > 192)
		error("Illegal value for \"l\" directive.");		// // //

	defaultNoteLength = i;
}

void Music::parseGlobalVolumeCommand() {
	int i = getInt();		// // //
	if (i == -1)
		error("Error parsing global volume (\"w\") command.");		// // //
	if (i < 0 || i > 255)
		error("Illegal value for global volume (\"w\") command.");		// // //

	append(AMKd::Binary::CmdType::VolGlobal, i);		// // //
}

void Music::parseVolumeCommand() {
	int i = getInt();		// // //
	if (i == -1)
		error("Error parsing volume (\"v\") command.");		// // //
	if (i < 0 || i > 255)
		error("Illegal value for global volume (\"v\") command.");		// // //

	append(AMKd::Binary::CmdType::Vol, i);		// // //
}

void Music::parseQuantizationCommand() {
	int i = getHex();		// // //
	if (i == -1)
		error("Error parsing quantization (\"q\") command.");		// // //
	if (i < 0 || i > 0x7F)		// // //
		error("Error parsing quantization (\"q\") command.");		// // //

	if (inNormalLoop) {		// // //
		tracks[prevChannel].q = i;		// // //
		tracks[prevChannel].updateQ = true;
	}
	else {
		tracks[channel].q = i;
		tracks[channel].updateQ = true;
	}

	tracks[CHANNELS].q = i;
	tracks[CHANNELS].updateQ = true;
}

void Music::parsePanCommand() {
	int pan = getInt();		// // //
	if (pan == -1)
		error("Error parsing pan (\"y\") command.");		// // //
	if (pan < 0 || pan > 20)
		error("Illegal value for pan (\"y\") command.");		// // //

	skipSpaces();
	if (trimChar(',')) {		// // //
		int i = getInt();
		if (i == -1)
			error("Error parsing pan (\"y\") command.");		// // //
		if (i > 2)
			error("Illegal value for pan (\"y\") command.");		// // //
		pan |= (i << 7);
		skipSpaces();
		if (!trimChar(','))
			error("Error parsing pan (\"y\") command.");		// // //
		i = getInt();
		if (i == -1)
			error("Error parsing pan (\"y\") command.");		// // //
		if (i > 2)
			error("Illegal value for pan (\"y\") command.");		// // //
		pan |= (i << 6);
	}

	append(AMKd::Binary::CmdType::Pan, pan);		// // //
}

void Music::parseIntroDirective() {
	if (inNormalLoop)		// // //
		error("Intro directive found within a loop.");		// // //

	if (hasIntro == false)
		tempoChanges.emplace_back(tracks[channel].channelLength, -static_cast<int>(tempo));		// // //
	else		// // //
		for (auto &x : tempoChanges)
			if (x.second < 0)
				x.second = -((int)tempo);

	hasIntro = true;
	tracks[channel].phrasePointers[1] = static_cast<uint16_t>(tracks[channel].data.size());		// // //
	prevNoteLength = -1;
	hasIntro = true;
	introLength = static_cast<unsigned>(tracks[channel].channelLength);		// // //
}

void Music::parseT() {
	if (trimDirective("uning["))		// // //
		parseTransposeDirective();
	else
		parseTempoCommand();
}

void Music::parseTempoCommand() {
	int i = getInt();		// // //
	if (i == -1)
		error("Error parsing tempo (\"t\") command.");		// // //
	if (i <= 0 || i > 255)
		error("Illegal value for tempo (\"t\") command.");		// // //

	i = divideByTempoRatio(i, false);

	if (i == 0)
		error("Tempo has been zeroed out by #halvetempo");		// // //

	tempo = i;
	tempoDefined = true;

	if (inNormalLoop || inE6Loop)		// // // Not even going to try to figure out tempo changes inside loops.  Maybe in the future.
		guessLength = false;
	else
		tempoChanges.emplace_back(tracks[channel].channelLength, tempo);		// // //

	append(AMKd::Binary::CmdType::Tempo, tempo);		// // //
}

void Music::parseTransposeDirective() {
	int i = getInt();		// // //
	if (i == -1)
		error("Error parsing tuning directive.");		// // //
	if (i < 0 || i > 255)
		error("Illegal instrument value for tuning directive.");		// // //

	if (!trimChar(']'))		// // //
		error("Error parsing tuning directive.");
	skipSpaces();
	if (!trimChar('='))
		error("Error parsing tuning directive.");

	while (true) {
		skipSpaces();

		bool plus = true;
		if (trimChar('-'))		// // //
			plus = false;
		else
			trimChar('+');

		int j = getInt();		// // //
		if (j == -1)
			error("Error parsing tuning directive.");		// // //
		if (!plus) j = -j;

		transposeMap[i] = j;

		skipSpaces();
		if (!trimChar(',')) break;		// // //
		if (++i >= 256)
			error("Illegal value for tuning directive.");
	}
}

void Music::parseOctaveDirective() {
	int i = getInt();		// // //
	if (i == -1)
		error("Error parsing octave (\"o\") directive.");		// // //
	if (i < 1 || i > 6)		// // //
		error("Error parsing octave (\"o\") directive.");		// // //

	octave = i;
}

void Music::parseInstrumentCommand() {
	bool direct = trimChar('@');		// // //

	int i = getInt();		// // //
	if (i == -1)
		error("Error parsing instrument (\"@\") command.");		// // //
	if (i < 0 || i > 255)
		error("Illegal value for instrument (\"@\") command.");		// // //

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

		if (songTargetProgram == TargetType::AM4)		// // //
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
	//transposeMap[tracks[channe].instrument] = ::tmpTrans[tracks[channe].instrument];
}

void Music::parseOpenParenCommand() {
	int i;		// // //
	if (trimChar('@')) {		// // //
		i = getInt();
		i = instrToSample[i];
		if (!trimChar(','))		// // //
			error("Error parsing sample load command.");
	}
	else if (trimChar('\"')) {		// // //
		std::string s = getEscapedString();		// // //
		if (!trimChar(','))		// // //
			error("Error parsing sample load command.");

		s = basepath + s;

		auto it = std::find(mySamples.cbegin(), mySamples.cend(), getSample(s, this));		// // //
		if (it == mySamples.cend())
			error("The specified sample was not included in this song.");
		i = std::distance(mySamples.cbegin(), it);
	}
	else {
		parseLabelLoopCommand();
		return;
	}

	int j;		// // //
	if (!getHexByte(j))		// // //
		error("Error parsing sample load command.");
	if (!trimChar(')'))
		error("Error parsing sample load command.");

	if (optimizeSampleUsage)
		usedSamples[i] = true;

	append(AMKd::Binary::CmdType::SampleLoad, i, j);		// // //
}

void Music::parseLabelLoopCommand() {
	if (trimChar('!')) {		// // //
		if (targetAMKVersion < 2)
			error("Unrecognized character '!'.");

		skipSpaces();

		if (channelDefined) {						// A channel's been defined, we're parsing a remote 
			//bool negative = false;
			int i = getInt();		// // //
			if (i == -1)
				error("Error parsing remote code setup.");		// // //
			skipSpaces();
			if (!trimChar(','))		// // //
				error("Error parsing code setup.");
			//if (peek() == '-') negative = true, skipChars(1);
			int j;		// // //
			try {
				j = getIntWithNegative();
			}
			catch (...) {
				error("Error parsing remote code setup. Remember that remote code cannot be defined within a channel.");
			}
			//if (negative) j = -j;
			skipSpaces();
			int k = 0;
			if (j == 1 || j == 2) {
				if (!trimChar(','))		// // //
					error("Error parsing remote code setup. Missing the third argument.");
				skipSpaces();
				if (trimChar('$')) {		// // //
					k = getHex();
					if (k == -1) error("Error parsing remote code setup. Could not parse the third argument as a hex command.");		// // //
				}
				else {
					k = getNoteLength();		// // //
					if (k > 0x100)
						error("Note length specified was too large.");		// // //
					else if (k == 0x100)
						k = 0;
				}

				skipSpaces();
			}

			if (!trimChar(')'))		// // //
				error("Error parsing remote setup.");

			if (trimChar('['))		// // //
				error("Remote code cannot be defined within a channel.");
			tracks[channel].loopLocations.push_back(static_cast<uint16_t>(tracks[channel].data.size() + 1));		// // //
			if (loopPointers.find(i) == loopPointers.cend())		// // //
				loopPointers.insert({i, -1});
			append(AMKd::Binary::CmdType::Callback, loopPointers[i] & 0xFF, loopPointers[i] >> 8, j, k);
			return;
		}
		else {								// We're outside of a channel, this is a remote call definition.
			int i = getInt();		// // //
			if (i == -1)
				error("Error parsing remote code definition.");
			skipSpaces();
			if (!trimChar(')'))		// // //
				error("Error parsing remote code definition.");

			if (peek() == '[') {
				loopLabel = i;
				// // //
				inRemoteDefinition = true;
				return;
			}
			error("Error parsing remote code definition; the definition was missing.");
		}
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

	tracks[channel].updateQ = true;
	tracks[CHANNELS].updateQ = true;
	prevNoteLength = -1;

	if (peek() == '[') {				// If this is a loop definition...
		loopLabel = i;				// Just set the loop label to this. The rest of the code is handled in the respective function.
		tracks[channel].isDefiningLabelLoop = true;		// // //
	}
	else {						// Otherwise, if this is a loop call...
		loopLabel = i;

		prevNoteLength = -1;

		auto it = loopPointers.find(loopLabel);		// // //
		if (it == loopPointers.cend())
			error("Label not yet defined.");
		int j = getInt();		// // //
		if (j == -1)
			j = 1;
		if (j < 1 || j > 255)
			error("Invalid loop count.");		// // //

		handleNormalLoopRemoteCall(j);

		tracks[channel].loopLocations.push_back(static_cast<uint16_t>(tracks[channel].data.size() + 1));		// // //
		append(AMKd::Binary::CmdType::Loop, it->second & 0xFF, it->second >> 8, j);

		loopLabel = 0;
	}
}

void Music::parseLoopCommand() {
	if (!inNormalLoop)		// // //
		tracks[channel].updateQ = true;
	tracks[CHANNELS].updateQ = true;
	prevNoteLength = -1;

	if (trimChar('[')) {		// // // This is an $E6 loop.
		if (trimChar('['))		// // //
			fatalError("An ambiguous use of the [ and [[ loop delimiters was found (\"[[[\").  Separate\n"
					   "the \"[[\" and \"[\" to clarify your intention.");
		if (inE6Loop)
			error("You cannot nest a subloop within another subloop.");
		if (loopLabel > 0 && tracks[channel].isDefiningLabelLoop)		// // //
			error("A label loop cannot define a subloop.  Use a standard or remote loop instead.");

		handleSuperLoopEnter();

		append(AMKd::Binary::CmdType::Subloop, 0x00);		// // //
		return;
	}
	else if (inNormalLoop)		// // //
		error("You cannot nest standard [ ] loops.");

	prevLoop = tracks[CHANNELS].data.size();		// // //

	prevChannel = channel;				// We're in a loop now, which is represented as channel 8.
	channel = CHANNELS;					// So we have to back up the current channel.
	inNormalLoop = true;		// // //
	prevNoteLength = -1;
	tracks[CHANNELS].instrument = tracks[prevChannel].instrument;		// // //
	if (songTargetProgram == TargetType::AM4)
		tracks[CHANNELS].ignoreTuning = tracks[prevChannel].ignoreTuning; // More AM4 tuning stuff.  Related to the line above it.

	if (loopLabel > 0 && loopPointers.find(loopLabel) != loopPointers.cend())		// // //
		error("Label redefinition.");

	if (loopLabel > 0)
		loopPointers[loopLabel] = prevLoop;

	handleNormalLoopEnter();
}

void Music::parseLoopEndCommand() {
	if (!inNormalLoop)		// // //
		tracks[channel].updateQ = true;		// // //

	tracks[CHANNELS].updateQ = true;
	prevNoteLength = -1;
	if (trimChar(']')) {		// // //
		if (trimChar(']'))
			fatalError(R"(An ambiguous use of the ] and ]] loop delimiters was found ("]]]").  Separate\nthe "]]" and "]" to clarify your intention.)");		// // //

		int i = getInt();		// // //
		if (i == 1)
			error("A subloop cannot only repeat once. It's pointless anyway.");
		if (i == -1)
			error("Error parsing subloop command; the loop count was missing.");		// // //
			//if (i == 0)
			//	error("A subloop cannot loop 0 times.")

		if (!inE6Loop)
			error("A subloop end was found outside of a subloop.");		// // //
		inE6Loop = false;

		handleSuperLoopExit(i);

		append(AMKd::Binary::CmdType::Subloop, i - 1);		// // //
		return;
	}
	else if (!inNormalLoop)		// // //
		error("Loop end found outside of a loop.");

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

	handleNormalLoopExit(i);

	if (!inRemoteDefinition) {
		tracks[channel].loopLocations.push_back(static_cast<uint16_t>(tracks[channel].data.size() + 1));		// // //
		append(AMKd::Binary::CmdType::Loop, prevLoop & 0xFF, prevLoop >> 8, i);
	}
	inRemoteDefinition = false;
	loopLabel = 0;
	tracks[channel].isDefiningLabelLoop = false;		// // //
}

void Music::parseStarLoopCommand() {
	if (inNormalLoop)		// // //
		error("Nested loops are not allowed.");		// // //

	tracks[channel].updateQ = true;
	tracks[CHANNELS].updateQ = true;
	prevNoteLength = -1;

	int i = getInt();		// // //
	if (i == -1)
		i = 1;
	if (i < 1 || i > 255)
		error("Invalid loop count.");

	handleNormalLoopRemoteCall(i);

	tracks[channel].loopLocations.push_back(static_cast<uint16_t>(tracks[channel].data.size() + 1));		// // //
	append(AMKd::Binary::CmdType::Loop, prevLoop & 0xFF, prevLoop >> 8, i);
}

void Music::parseVibratoCommand() {
	int t1, t2, t3;
	t1 = getInt();
	if (t1 == -1) error("Error parsing vibrato command.");
	skipSpaces();
	if (!trimChar(','))		// // //
		error("Error parsing vibrato command.");
	t2 = getInt();
	if (t2 == -1) error("Error parsing vibrato command.");
	skipSpaces();

	if (trimChar(',')) {	// The user has specified the delay.
		t3 = getInt();
		if (t3 == -1) error("Error parsing vibrato command.");
		if (t1 < 0 || t1 > 255) error("Illegal value for vibrato delay.");		// // //
		if (t2 < 0 || t2 > 255) error("Illegal value for vibrato rate.");
		if (t3 < 0 || t3 > 255) error("Illegal value for vibrato extent.");

		append(AMKd::Binary::CmdType::Vibrato, divideByTempoRatio(t1, false), multiplyByTempoRatio(t2), t3);		// // //
	}
	else {			// The user has not specified the delay.
		if (t1 < 0 || t1 > 255) error("Illegal value for vibrato rate.");
		if (t2 < 0 || t2 > 255) error("Illegal value for vibrato extent.");

		append(AMKd::Binary::CmdType::Vibrato, 0x00, multiplyByTempoRatio(t1), t2);		// // //
	}
}

void Music::parseTripletOpenDirective() {
	if (triplet == false)
		triplet = true;
	else
		error("Triplet on directive found within a triplet block.");
}

void Music::parseTripletCloseDirective() {
	if (triplet == true)
		triplet = false;
	else
		error("Triplet off directive found outside of a triplet block.");
}

void Music::parseRaiseOctaveDirective() {
	if (++octave > 7) {
		octave = 7;
		error("The octave has been raised too high.");
	}
}

void Music::parseLowerOctaveDirective() {
	if (--octave < 1) {		// // //
		octave = 1;
		error("The octave has been dropped too low.");
	}
}

void Music::parsePitchSlideCommand() {
	if (!inPitchSlide)
		inPitchSlide = true;
	else
		error("Pitch slide directive specified multiple times in a row.");
}

void Music::parseHFDInstrumentHack(int addr, int bytes) {
	int byteNum = 0;
	do {
		int i;		// // //
		if (!getHexByte(i))		// // //
			error("Unknown HFD hex command.");
		instrumentData.push_back(i);
		bytes--;
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
	} while (bytes >= 0);
}

void Music::parseHFDHex() {
	int i;		// // //
	if (!getHexByte(i))		// // //
		error("Unknown HFD hex command.");

	if (convert) {
		switch (i) {
		case 0x80:
		{
			int reg, val;
			if (!getHexByte(reg) || !getHexByte(val))		// // //
				error("Error while parsing HFD hex command.");

			if (!(reg == 0x6D || reg == 0x7D))	// Do not write the HFD header hex bytes.
				if (reg == 0x6C)			// Noise command gets special treatment.
					append(AMKd::Binary::CmdType::Noise, val);		// // //
				else
					append(AMKd::Binary::CmdType::DSP, reg, val);		// // //
			else
				songTargetProgram = TargetType::AM4;		// // // The HFD header bytes indicate this as being an AM4 song, so it gets AM4 treatment.
		} break;
		case 0x81:
			if (!getHexByte(i))		// // //
				error("Error while parsing HFD hex command.");
			append(AMKd::Binary::CmdType::ExtFA, AMKd::Binary::CmdOptionFA::Transpose, i);		// // //
			break;
		case 0x82:
		{
			int addrHi, addrLo;
			int bytesHi, bytesLo;
			if (!getHexByte(addrHi) || !getHexByte(addrLo) || !getHexByte(bytesHi) || !getHexByte(bytesLo))		// // //
				error("Error while parsing HFD hex command.");
			int addr = (addrHi << 8) | addrLo;
			int bytes = (bytesHi << 8) | bytesLo;

			if (addr == 0x6136) {
				parseHFDInstrumentHack(addr, bytes);
				return;
			}

			for (++bytes; bytes > 0; --bytes) {		// // //
				if (!getHexByte(i))
					error("Error while parsing HFD hex command.");
				// append(AMKd::Binary::CmdType::ARAM, i, addr >> 8, addr & 0xFF);		// // // Don't do this stuff; we won't know what we're overwriting.
				++addr;
			}
		} break;
		case 0x83:
			error("HFD hex command $ED$83 not implemented.");
		}

		hexLeft = 0;
		return;
	}

	currentHex = AMKd::Binary::CmdType::Envelope;
	hexLeft = hexLengths[currentHex - AMKd::Binary::CmdType::Inst] - 1 - 1;
	append(AMKd::Binary::CmdType::Envelope, i);		// // //
}

// // //
void Music::insertRemoteConversion(std::vector<uint8_t> &&cmd, std::vector<uint8_t> &&conv) {
	auto index = static_cast<uint16_t>(tracks[channel].data.size() + 1);
	tracks[channel].remoteGainInfo.emplace_back(index, std::move(cmd));		// // //
	tracks[channel].data.insert(tracks[channel].data.cend(), conv.cbegin(), conv.cend());
}

// // //
void Music::removeRemoteConversion() {
	tracks[channel].remoteGainInfo.pop_back();		// // //
}

//static bool validateTremolo;

static bool nextNoteIsForDD;

void Music::parseHexCommand() {
	int i = getHex();		// // //
	if (i < 0 || i > 0xFF)
		error("Error parsing hex command.");

	if (!validateHex) {
		append(i);
		return;
	}

	if (hexLeft == 0) {
		//validateTremolo = true;
		currentHex = i;

		switch (currentHex) {		// // //
		case AMKd::Binary::CmdType::TempoFade:
			guessLength = false; // NOPE.  Nope nope nope nope nope nope nope nope nope nope.
			break;
		case AMKd::Binary::CmdType::Tremolo:
			if (songTargetProgram == TargetType::AM4) {
				// Don't append yet.  We need to look at the next byte to determine whether to use 0xF3 or 0xE5.
				hexLeft = 3;
				return;
			}
			break;
		case AMKd::Binary::CmdType::Envelope:
			if (songTargetProgram == TargetType::AM4) {
				parseHFDHex();
				return;
			}
			break;
		case AMKd::Binary::CmdType::Arpeggio:
		{
			int j;
			if (!getHexByte(j))		// // //
				error("Error parsing hex command.");
			if (j >= 0x80)
				hexLeft = 2;
			else
				hexLeft = j + 1;
			nextHexIsArpeggioNoteLength = true;
			append(currentHex, j);
		}
			return;
		case AMKd::Binary::CmdType::Callback:
			if (targetAMKVersion > 1)
				error("$FC has been replaced with remote code in #amk 2 and above.");		// // //
			else if (targetAMKVersion == 1) {
				//if (tempoRatio != 1) error("#halvetempo cannot be used on AMK 1 songs that use the $FA $05 or old $FC command.")
					// Add in a "restore instrument" remote call.
				int channelToCheck = inNormalLoop ? prevChannel : channel;		// // //
				tracks[channelToCheck].usingFC = true;		// // //

				// If we're just using the FC command and not the FA command as well,
				if (!tracks[channelToCheck].usingFA) {
					// Then add the "restore instrument command"
					insertRemoteConversion({AMKd::Binary::CmdType::ExtF4, AMKd::Binary::CmdOptionF4::RestoreInst, 0x00},
										   {AMKd::Binary::CmdType::Callback, 0x00, 0x00, AMKd::Binary::CmdOptionFC::KeyOn, 0x00});

					// Then add in the first part of a "apply gain before a note ends" call.
					hexLeft = 2;
					insertRemoteConversion({AMKd::Binary::CmdType::ExtFA, AMKd::Binary::CmdOptionFA::Gain},
										   {AMKd::Binary::CmdType::Callback, 0x00, 0x00, AMKd::Binary::CmdOptionFC::Release});

					// We won't know the gain and delays until later.
				}
				else {
					// Then add in the first part of a "2 3 combination" call.  
					// Theoretically we could go back and change the previous type to 5.
					// But that's annoying if the commands are separated, so maybe some other time.
					// Shh.  Don't tell anyone.

					hexLeft = 2;
					insertRemoteConversion({AMKd::Binary::CmdType::ExtFA, AMKd::Binary::CmdOptionFA::Gain},
										   {AMKd::Binary::CmdType::Callback, 0x00, 0x00, 0x05});
					//append(lastFAGainValue[channelToCheck]);
				}
				return;
			}
		case 0xFD: case 0xFE: case 0xFF:
			error("Unknown hex command.");
		default:
			if (currentHex < AMKd::Binary::CmdType::Inst && manualNoteWarning) {
				if (targetAMKVersion == 0) {
					printWarning("Warning: A hex command was found that will act as a note instead of a special\n"
								 "effect. If this is a song you're using from someone else, you can most likely\n"
								 "ignore this message, though it may indicate that a necessary #amm or #am4 is\n"
								 "missing.", name, line);
					manualNoteWarning = false;
					return;
				}
				error("Unknown hex command.");
			}
		}

		hexLeft = hexLengths[currentHex - AMKd::Binary::CmdType::Inst] - 1;
		append(currentHex);

		/*
		while (hexLeft > 0) {
			int param;
			if (!getHexByte(param))
				error("Incomplete hex command.");

			if (peek() != '$') {
				if (songTargetProgram == TargetType::AM4 && currentHex == AMKd::Binary::CmdType::Subloop) {		// // //
					// tremolo off
					tracks[channel].data.pop_back();
					append(AMKd::Binary::CmdType::Tremolo, 0x00, 0x00, 0x00);
					hexLeft = 0;
				}
				else
					error("Incomplete hex command.");
			}
		}
		*/
	}
	else {
		--hexLeft;
		switch (currentHex) {
		case AMKd::Binary::CmdType::Inst:
			if (songTargetProgram == TargetType::AM4) {		// // // If this was the instrument command
				if (i >= 0x13)					// And it was >= 0x13
					i = i - 0x13 + 30;		// Then change it to a custom instrument.
				if (optimizeSampleUsage)
					usedSamples[instrumentData[(i - 30) * 5]] = true;
			}
			break;
		case AMKd::Binary::CmdType::PanFade:
			if (hexLeft == 1)
				i = divideByTempoRatio(i, false);
			break;
		case AMKd::Binary::CmdType::Portamento:
			if (hexLeft == 2)
				i = divideByTempoRatio(i, false);
			else if (hexLeft == 1) {		// Hack allowing the $DD command to accept a note as a parameter.
					std::string backUpText = text;		// // //
					bool finished = false;
					while (!finished) {
						skipSpaces();
						switch (peek()) {
						case 'o':
							getInt(); break;
						case 'c': case 'd': case 'e': case 'f': case 'g': case 'a': case 'b':
						case 'C': case 'D': case 'E': case 'F': case 'G': case 'A': case 'B':
							if (tracks[channel].updateQ)		// // //
								error("You cannot use a note as the last parameter of the $DD command if you've also\n"
									  "used the qXX command just before it.");		// // //
							hexLeft = 0;
							nextNoteIsForDD = true;
							finished = true; break;
						case '<': case '>':
							skipChars(1); break;
						default:
							finished = true; break;
						}
					}
					text = backUpText;		// // //
				i = divideByTempoRatio(i, false);
			}
			break;
		case AMKd::Binary::CmdType::Vibrato:
			if (hexLeft == 2)
				i = divideByTempoRatio(i, false);
			else if (hexLeft == 1)
				i = multiplyByTempoRatio(i);
			break;
		case AMKd::Binary::CmdType::VolGlobalFade:
			if (hexLeft == 1)
				i = divideByTempoRatio(i, false);
			break;
		case AMKd::Binary::CmdType::Tempo:
			if (hexLeft == 0)
				i = divideByTempoRatio(i, false);
			break;
		case AMKd::Binary::CmdType::TempoFade:
			if (hexLeft == 1)
				i = divideByTempoRatio(i, false);
			break;
		case AMKd::Binary::CmdType::TrspGlobal:
			if (hexLeft == 0 && songTargetProgram == TargetType::AM4) {	// // // AM4 seems to do something strange with $E4?
				++i;
				i &= 0xFF;
			}
			break;
		case AMKd::Binary::CmdType::Tremolo:
			// If we're on the last hex value for $E5 and this isn't an AMK song, then do some special stuff regarding tremolo.
			// AMK doesn't use $E5 for the tremolo command or sample loading, so it has to emulate them.
			if (hexLeft == 2) {		// // //
				if (songTargetProgram == TargetType::AM4/*validateTremolo*/) {
					//validateTremolo = false;
					if (i >= 0x80) {
						--hexLeft;
						if (mySamples.empty() && (i & 0x7F) > 0x13)
							error("This song uses custom samples, but has not yet defined its samples with the #samples command.");		// // //

						if (optimizeSampleUsage)
							usedSamples[i - 0x80] = true;
						append(AMKd::Binary::CmdType::SampleLoad, i - 0x80);		// // //
						return;
					}
					else
						append(AMKd::Binary::CmdType::Tremolo);
				}
				i = divideByTempoRatio(i, false);
			}
			else if (hexLeft == 1)
				i = multiplyByTempoRatio(i);
			break;
		case AMKd::Binary::CmdType::Subloop:
			if (hexLeft == 0) {
				if (i == 0) {
					if (inE6Loop)
						error("Cannot nest $E6 loops within other $E6 loops.");
					inE6Loop = true;
					handleSuperLoopEnter();
				}
				else {
					if (!inE6Loop)
						error("An E6 loop starting point has not yet been declared.");
					inE6Loop = false;
					handleSuperLoopExit(i + 1);
				}
			}
			break;
		case AMKd::Binary::CmdType::VolFade:
			if (hexLeft == 1)
				i = divideByTempoRatio(i, false);
			break;
		case AMKd::Binary::CmdType::VibratoFade:
			if (hexLeft == 0)
				i = divideByTempoRatio(i, false);
			break;
		case AMKd::Binary::CmdType::BendAway:
			if (hexLeft == 2 || hexLeft == 1)
				i = divideByTempoRatio(i, false);
			break;
		case AMKd::Binary::CmdType::BendTo:
			if (hexLeft == 2 || hexLeft == 1)
				i = divideByTempoRatio(i, false);
			break;
		case AMKd::Binary::CmdType::Echo2:
			if (hexLeft == 2)
				echoBufferSize = std::max(echoBufferSize, i);
			// Print error for AM4 songs that attempt to use an invalid FIR filter. They both
			// A) won't sound like their originals and
			// B) may crash the DSP (or for whatever reason that causes SPCPlayer to go silent with them).
			else if (hexLeft == 0 && songTargetProgram == TargetType::AM4) {		// // //
				if (i > 1) {
					char buffer[4];		// // //
					sprintf_s(buffer, "$%02X", i);
					error(buffer + (std::string)" is not a valid FIR filter for the $F1 command. Must be either $00 or $01.");		// // //
				}
			}
			break;
		case AMKd::Binary::CmdType::EchoFade:
			if (hexLeft == 2)
				i = divideByTempoRatio(i, false);
			break;
		case AMKd::Binary::CmdType::SampleLoad:
			if (hexLeft == 1)
				if (optimizeSampleUsage)
					usedSamples[i] = true;
			break;
		case AMKd::Binary::CmdType::ExtF4:
			if (hexLeft == 0)
				if (i == AMKd::Binary::CmdOptionF4::YoshiCh5 || i == AMKd::Binary::CmdOptionF4::Yoshi)
					hasYoshiDrums = true;
			break;
		case AMKd::Binary::CmdType::ExtFA:
			if (hexLeft == 1 && targetAMKVersion > 1 && i == 0x05)
				error("$FA $05 in #amk 2 or above has been replaced with remote code.");		// // //
			// More code conversion.
			if (hexLeft == 0 && targetAMKVersion == 1 && tracks[channel].data.back() == 0x05) {
				//if (tempoRatio != 1) error("#halvetempo cannot be used on AMK 1 songs that use the $FA $05 or old $FC command.")
				tracks[channel].data.pop_back();					// // // Remove the last two bytes
				tracks[channel].data.pop_back();					// (i.e. the $FA $05)

				int channelToCheck = inNormalLoop ? prevChannel : channel;

				if (i != 0) {
					// Check if this channel is using FA and FC combined...
					if (!tracks[channelToCheck].usingFC) {		// // //
						// Then add in a "restore instrument" remote call.
						insertRemoteConversion({AMKd::Binary::CmdType::ExtF4, AMKd::Binary::CmdOptionF4::RestoreInst, 0x00},
											   {AMKd::Binary::CmdType::Callback, 0x00, 0x00, AMKd::Binary::CmdOptionFC::KeyOn, 0x00});

						// Then add the "set gain" remote call.
						insertRemoteConversion({AMKd::Binary::CmdType::ExtFA, AMKd::Binary::CmdOptionFA::Gain, static_cast<uint8_t>(i), 0x00},
											   {AMKd::Binary::CmdType::Callback, 0x00, 0x00, AMKd::Binary::CmdOptionFC::KeyOff, 0x00});
					}
					else {
						// Otherwise, add in a "2 5 combination" command.

						// Then add the "set gain" remote call.
						insertRemoteConversion({AMKd::Binary::CmdType::ExtFA, AMKd::Binary::CmdOptionFA::Gain, static_cast<uint8_t>(i), 0x00},
											   {AMKd::Binary::CmdType::Callback, 0x00, 0x00, 0x05, tracks[channelToCheck].lastFCDelayValue});
						//append(0x00);
					}

					// Either way, we're using FA now.
					tracks[channelToCheck].usingFA = true;
				}
				else {
					insertRemoteConversion({},
										   {AMKd::Binary::CmdType::Callback, 0x00, 0x00, AMKd::Binary::CmdOptionFC::Disable, 0x00});
					tracks[channelToCheck].usingFA = false;
				}
				return;
			}
			break;
		case AMKd::Binary::CmdType::Arpeggio:
			if (nextHexIsArpeggioNoteLength) {
				i = divideByTempoRatio(i, false);
				nextHexIsArpeggioNoteLength = false;
			}
			break;
		case AMKd::Binary::CmdType::Callback:
			// Do conversion for the old remote gain command.
			if (hexLeft == 1 && targetAMKVersion == 1) {
				//if (tempoRatio != 1) error("#halvetempo cannot be used on AMK 1 songs that use the $FA $05 or old $FC command.")

				int channelToCheck = inNormalLoop ? prevChannel : channel;		// // //

				if (i == 0) {							// If i is zero, we have to undo a bunch of stuff.
					if (!tracks[channelToCheck].usingFA) {		// // // But only if this is a "pure" FC command.
						removeRemoteConversion();
						removeRemoteConversion();

						tracks[channel].data.pop_back();		// // //
						tracks[channel].data.pop_back();
						tracks[channel].data.pop_back();
						tracks[channel].data.pop_back();
						tracks[channel].data.pop_back();
						tracks[channel].data.pop_back();
						tracks[channel].data.pop_back();
						tracks[channel].data.pop_back();
						tracks[channel].data.pop_back();

						insertRemoteConversion({},
											   {AMKd::Binary::CmdType::Callback, 0x00, 0x00, AMKd::Binary::CmdOptionFC::Disable, 0x00});
					}
					else {
						// If we're using FA and FC, then we need to "restore" the FA data.

						// Same as the other "get rid of stuff", but without the "restore instrument" call.
						removeRemoteConversion();

						tracks[channel].data.pop_back();		// // //
						tracks[channel].data.pop_back();
						tracks[channel].data.pop_back();
						tracks[channel].data.pop_back();

						// Then add the "set gain" remote call.
						insertRemoteConversion({AMKd::Binary::CmdType::ExtFA, AMKd::Binary::CmdOptionFA::Gain, static_cast<uint8_t>(i), 0x00},
											   {AMKd::Binary::CmdType::Callback, 0x00, 0x00, AMKd::Binary::CmdOptionFC::KeyOff, 0x00});
					}

					// Either way, FC gets turned off.
					tracks[channelToCheck].usingFC = false;

					//remoteGainConversion.back().push_back(AMKd::Binary::CmdType::ExtFA);
					//remoteGainConversion.back().push_back(AMKd::Binary::CmdOptionFA::Gain);
				}
				else {
					i = divideByTempoRatio(i, false);
					tracks[channelToCheck].lastFCDelayValue = static_cast<uint8_t>(i);		// // //
					append(i);
				}
				return;
			}
			else if (hexLeft == 0 && targetAMKVersion == 1) {		// // //
				//if (tempoRatio != 1) error("#halvetempo cannot be used on AMK 1 songs that use the $FA $05 or old $FC command.")
				// // //
				std::map<std::vector<uint8_t>, uint16_t> a;
				if (!tracks[channel].remoteGainInfo.empty()) {
					auto &conv = tracks[channel].remoteGainInfo.back().second;		// // //
					if (!conv.empty()) {			// If the size was zero, then it has no data anyway.  Used for the 0 event type.
	//					int channelToCheck = inNormalLoop ? prevChannel : channel;			// Only saves two bytes, though.

	//					tracks[channelToCheck].lastFCGainValue = static_cast<uint8_t>(i);		// // //
						conv.push_back(i);
						conv.push_back(0x00);
					}
				}
				return;
			}
			break;
		}

		if (i == -1)
			error("Error parsing hex command.");
		if (i < 0 || i > 255)
			error("Illegal value for hex command.");
		append(i);
	}
}

void Music::parseNote(int ch) {		// // //
	if (inRemoteDefinition)
		error("Remote definitions cannot contain note data!");

	// // //
	if (songTargetProgram == TargetType::AMK && channelDefined == false && inRemoteDefinition == false)
		error("Note data must be inside a channel!");

	int i;		// // //
	if (ch == 'r')
		i = AMKd::Binary::CmdType::Rest;		// // //
	else if (ch == '^')
		i = AMKd::Binary::CmdType::Tie;
	else {
		//am4silence++;
		i = getPitch(ch);

		if (usingHTranspose)
			i += hTranspose;
		else {
			if (!tracks[channel].ignoreTuning)		// // // More AM4 tuning stuff
				i -= transposeMap[tracks[channel].instrument];
		}

		if (i < 0)
			error("Note's pitch was too low.");		// // //
		else if (i >= AMKd::Binary::CmdType::Tie)
			error("Note's pitch was too high.");
		else if (tracks[channel].instrument >= 21 && tracks[channel].instrument < 30)		// // //
		{
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

	int j = getNoteLength();		// // //
	skipSpaces();

	while (true) {
		std::string temptext = text;		// // //
		if (!trimChar('^') && !(i == AMKd::Binary::CmdType::Rest && trimChar('r')))
			break;

		int tempsize = j;	// If there's a pitch bend up ahead, we need to not optimize the last tie.
		j += getNoteLength();
		skipSpaces();

		if (strnicmp(text.c_str(), "$DD", 3) == 0) {
			j = tempsize;		//
			text = temptext;		// // // "Rewind" so we forcibly place a tie before the bend.
			break;			//
		}
	}

	/*if (normalLoopInsideE6Loop)
	tempLoopLength += j;
	else if (normalLoopInsideE6Loop)
	e6LoopLength += j;
	else if (::inE6Loop)
	e6LoopLength += j;
	else if (inNormalLoop)
	tempLoopLength += j;
	else
	lengths[channel] += j;*/

	j = divideByTempoRatio(j, true);

	addNoteLength(j);

	if (j >= divideByTempoRatio(0x80, true)) {		// Note length must be less than 0x80
		append(divideByTempoRatio(0x60, true));

		if (tracks[channel].updateQ) {		// // //
			append(tracks[channel].q);
			tracks[channel].updateQ = false;
			tracks[CHANNELS].updateQ = false;
			noteParamaterByteCount++;
		}
		append(i); j -= divideByTempoRatio(0x60, true);

		while (j > divideByTempoRatio(0x60, true)) {
			append(AMKd::Binary::CmdType::Tie);		// // //
			j -= divideByTempoRatio(0x60, true);
		}

		if (j > 0) {
			if (j != divideByTempoRatio(0x60, true))
				append(j);
			append(AMKd::Binary::CmdType::Tie);		// // //
		}
		prevNoteLength = j;
		return;
	}
	else if (j > 0) {
		if (j != prevNoteLength || tracks[channel].updateQ)		// // //
			append(j);
		prevNoteLength = j;
		if (tracks[channel].updateQ) {
			append(tracks[channel].q);
			tracks[channel].updateQ = false;
			tracks[CHANNELS].updateQ = false;		// // //
			noteParamaterByteCount++;
		}
		append(i);
	}
	//append(i);
}

void Music::parseHDirective() {
	//bool negative = false;

	//if (peek() == '-') 
	//{
	//	negative = true;
	//	skipChars(1);
	//}
	try {
		int i = getIntWithNegative();		// // //
		//if (negative) i = -i;
		//transposeMap[tracks[channe].instrument] = -i;		// // //
		//htranspose[tracks[channe].instrument] = true;
		hTranspose = i;
		usingHTranspose = true;
	}
	catch (...) {
		error("Error parsing h transpose directive.");
	}
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
			append(AMKd::Binary::CmdType::ExtFA, AMKd::Binary::CmdOptionFA::VolTable, 0x00);		// // //
			usingSMWVTable = true;
		}
		else
			printWarning("This song is already using the SMW V Table. This command is just wasting three bytes...", name, line);
	else if (trimDirective("nspcvtable")) {		// // //
		append(AMKd::Binary::CmdType::ExtFA, AMKd::Binary::CmdOptionFA::VolTable, 0x01);		// // //
		usingSMWVTable = false;
		printWarning("This song uses the N-SPC V by default. This command is just wasting two bytes...", name, line);
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
			printWarning("#option dividetempo 1 has no effect besides printing this warning.", name, line);
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
	else if (trimDirective("define"))		// // //
		parseDefine();
	else if (trimDirective("undef"))		// // //
		parseUndef();
	else if (trimDirective("ifdef"))		// // //
		parseIfdef();
	else if (trimDirective("ifndef"))		// // //
		parseIfndef();
	else if (trimDirective("endif"))		// // //
		parseEndif();
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
	else if (trimDirective("am4"))		// // //
		;
	else if (trimDirective("amm"))		// // //
		;
	else if (trimDirective("amk="))		// // //
		getInt();
	else if (trimDirective("halvetempo")) {		// // //
		if (channelDefined)
			error("#halvetempo must be used before any and all channels.");		// // //
		tempoRatio *= 2;

		if (tempoRatio < 0)
			error("#halvetempo has been used too many times...what are you even doing?");		// // //
	}
	else if (trimDirective("option"))		// // //
		parseOptionDirective();
}

void Music::parseReplacementDirective() {
	std::string s = getEscapedString();		// // //
	int i = s.find('=');
	if (i == -1)
		printError("Error parsing replacement directive; could not find '='", true, name, line);

	std::string findStr = s.substr(0, i);
	std::string replStr = s.substr(i + 1);

	// // //
	if (!findStr.empty())
		while (isspace(findStr.back()))
			findStr.pop_back();
	if (findStr.empty())
		fatalError("Error parsing replacement directive; string to find was of zero length.");

	if (!replStr.empty())
		while (isspace(replStr.front()))
			replStr.erase(0, 1);

	auto it = replacements.find(findStr);
	if (it != replacements.end())
		replacements.erase(it);

	// keep substituting existing lexical macros in the result string; if the
	// original string appears as a substring of any intermediate result, then
	// the macro must fire an infinite recursion due to the macros alone
	// any macro that ultimately consumes external text cannot loop, so this
	// check is sufficient
	std::string replNew = replStr;
	for (std::size_t p = 0; p < replNew.size(); ++p) {
		auto it = std::find_if(replacements.cbegin(), replacements.cend(), [&] (const auto &x) {
			const std::string &rhs = x.first;
			std::size_t len = rhs.length();
			return p + len <= replNew.size() && std::string_view(replNew.c_str() + p, len) == rhs;
		});
		if (it == replacements.cend())
			continue;
		replNew.replace(replNew.begin() + p, replNew.begin() + p + it->first.length(),
						it->second.begin(), it->second.end());
		if (replNew.find(findStr, p) != std::string::npos)
			fatalError("Using this replacement macro will lead to infinite recursion.");
	}

	// repeat for the existing replacement macros, against the new one
	/*
	const std::size_t flen = findStr.length();
	for (auto &it : replacements) {
		const std::string &fi = it.first;
		std::string &re = it.second;
		std::size_t p = 0;
		while (true) {
			p = re.find(findStr, p);
			if (p == std::string::npos)
				break;
			re.replace(re.begin() + p, re.begin() + p + findStr.length(),
					   replNew.begin(), replNew.end());
			// ???
		}
	}
	*/

	replacements[findStr] = replNew;
}

void Music::parseInstrumentDefinitions() {
	skipSpaces();
	if (!trimChar('{'))		// // //
		fatalError("Could not find opening curly brace in instrument definition.");

	skipSpaces();
	while (!trimChar('}')) {
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
		skipSpaces();
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
	skipSpaces();

	if (!trimChar('{'))		// // //
		fatalError("Unexpected character in sample group definition.  Expected \"{\".");

	while (true) {		// // //
		skipSpaces();
		if (trimChar('\"')) {		// // //
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
	skipSpaces();
	if (trimChar('$')) {		// // //
		int i = getHex(true);
		if (i != -1) {
			minSize = i;
			return;
		}
	}

	error("Error parsing padding directive.");
}

void Music::parseLouderCommand() {
	append(AMKd::Binary::CmdType::ExtF4, AMKd::Binary::CmdOptionF4::VelocityTable);		// // //
}

void Music::parsePath() {
	skipSpaces();
	if (!trimChar('\"'))
		fatalError("Unexpected symbol found in path command. Expected a quoted string.");
	auto str = getEscapedString();		// // //
	if (str.empty())
		fatalError("Unexpected symbol found in path command. Expected a quoted string.");
	basepath = "./" + str + "/";
}

// // //
bool Music::hasNextToken() {
	auto &text_ = text;
	skipSpaces();
	if (!doReplacement(text_))
		fatalError("Infinite replacement macro substitution.");

	skipSpaces();
	return !text_.empty();
}

// // //
int Music::peek() {
	auto &text_ = text;
	if (text_.empty())
		return std::string::npos;
	return text_.front();
}

int Music::getInt() {
	//if (peek() == '$') { skipChars(1); return getHex(); }	// Allow for things such as t$20 instead of t32.
	// Actually, can't do it.
	// Consider this:
	// l8r$ED$00$00
	// Yeah. Oh well.
	// Attempt to do a replacement.  (So things like "ab = 8"    [c1]ab    are valid).
	auto &text_ = text;
	if (!hasNextToken())		// // //
		return -1;

	int i = 0;
	int d = 0;

	while (!text_.empty() && text_.front() >= '0' && text_.front() <= '9') {		// // //
		d++;
		i = (i * 10) + text_.front() - '0';
		skipChars(1);
	}

	return d ? i : -1;
}

int Music::getInt(const std::string &str, int &p) {
	int i = 0;
	int d = 0;

	while (p < static_cast<int>(str.size()) && str[p] >= '0' && str[p] <= '9') {		// // //
		d++;
		i = (i * 10) + str[p] - '0';
		p++;
	}

	if (d == 0) return -1; else return i;
}

int Music::getIntWithNegative() {
	auto &text_ = text;
	if (!hasNextToken())		// // //
		return -1;

	int i = 0;
	int d = 0;
	bool n = trimChar('-');		// // //

	while (!text_.empty() && text_.front() >= '0' && text_.front() <= '9') {		// // //
		d++;
		i = (i * 10) + text_.front() - '0';
		skipChars(1);
	}

	if (d == 0)
		throw "Invalid number";
	return n ? -i : i;
}

int Music::getHex(bool anyLength) {
	auto &text_ = text;
	if (!hasNextToken())		// // //
		return -1;

	int i = 0;
	int d = 0;
	int j;

	while (!text_.empty()) {		// // //
		if (d >= 2 && (!anyLength || songTargetProgram == TargetType::AM4))
			break;

		char ch = text_.front();
		if ('0' <= ch && ch <= '9')
			j = ch - '0';
		else if ('A' <= ch && ch <= 'F')
			j = ch - '7';
		else if ('a' <= ch && ch <= 'f')
			j = ch - ('7' + 'a' - 'A');
		else
			break;
		skipChars(1);

		++d;
		i = i * 16 + j;
	}

	return d != 0 ? i : -1;
}

// // //
bool Music::getHexByte(int &out) {
	auto &text_ = text;
	if (!hasNextToken())		// // //
		return false;

	if (!trimChar('$'))
		return false;
	int x = getHex();
	if (x < 0 || x > 0xFF)
		return false;
	out = x;
	return true;
}

int Music::getPitch(int i) {
	static const int pitches[] = {9, 11, 0, 2, 4, 5, 7};

	i = pitches[i - 'a'] + (octave - 1) * 12 + 0x80;		// // //
	if (trimChar('+')) ++i;
	else if (trimChar('-')) --i;

	/*if (i < 0x80)
	return -1;
	if (i >= 0xC6)
	return -2;*/

	return i;
}

// // //
int Music::getNoteLength() {
	//bool still = true;

	int len = getInt();		// // //

	if (len == -1 && trimChar('=')) {		// // //
		len = getInt();
		if (len == -1 || len == 0)		// // // do not allow zero duration
			printError("Error parsing note", false, name, line);
		return len;
		//if (len < 1) still = false; else return len;
	}

	//if (still)
	//{
	if (len < 1 || len > 192)
		len = defaultNoteLength;
	len = 192 / len;

	int frac = len;

	int times = 0;
	while (trimChar('.')) {		// // //
		frac /= 2;
		len += frac;
		if (++times == 2 && songTargetProgram == TargetType::AM4)
			break;	// // // AM4 only allows two dots for whatever reason.
	}

	if (triplet)
		len = static_cast<int>(std::floor(len * 2.0 / 3.0 + 0.5));
	return len;
}

// // //
std::string Music::getIdentifier() {
	std::string str;
	auto &text_ = text;
	size_t pos = 0, n = text_.size();
	while (pos < n && !::isspace(text_[pos]))
		str += text_[pos++];
	skipChars(pos);
	return str;
}

// // //
std::string Music::getEscapedString() {
	std::string tempstr;	
	auto &text_ = text;

	while (true) {
		if (text_.empty()) {
			printError("Unexpected end of file found.", false);
			return tempstr;
		}

		if (trimChar('\"'))
			break;

		char ch = text_.front();
		skipChars(1);
		if (ch == '\\') {
			if (trimChar('\"'))
				tempstr += '\"';
			else {
				printError(R"(Error: The only escape sequence allowed is "\"".)", false);		// // //
				return tempstr;
			}
		}
		else
			tempstr += ch;
	}

	return tempstr;
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

	playOnce = doesntLoop;

	spaceForPointersAndInstrs = 20;

	if (hasIntro)
		spaceForPointersAndInstrs += 18;
	if (!doesntLoop)
		spaceForPointersAndInstrs += 2;

	spaceForPointersAndInstrs += instrumentData.size();

	allPointersAndInstrs.resize(spaceForPointersAndInstrs);// = alloc(spaceForPointersAndInstrs);
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

	totalSize = spaceForPointersAndInstrs;
	std::for_each(std::cbegin(tracks), std::cend(tracks), [&] (const Track &t) { totalSize += t.data.size(); });		// // //

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
				printWarning("A tempo change was found beyond the end of the song.", name, line);
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
			seconds = (unsigned int)std::floor(l1 + l2 * 2 + 0.5);	// Just 2? Not 2.012584 or something?  Wow.
			mainSeconds = l2;
			introSeconds = l1;
		}
		else {
			mainSeconds = l1;
			seconds = (unsigned int)std::floor(l1 * 2 + 0.5);
		}
		knowsLength = true;
	}

	int spaceUsedBySamples = 0;
	for (const uint16_t x : mySamples)		// // //
		spaceUsedBySamples += 4 + samples[x].data.size();	// The 4 accounts for the space used by the SRCN table.

	if (verbose)
		std::cout << name << " total size: 0x" << hex4 << totalSize << " bytes" << std::dec << std::endl;
	else
		printChannelDataNonVerbose(totalSize);

	//for (int z = 0; z <= CHANNELS; z++)
	//{
	if (verbose) {
		// // //
		printf("\t#0: 0x%03X #1: 0x%03X #2: 0x%03X #3: 0x%03X Ptrs+Instrs: 0x%03X\n"
			   "\t#4: 0x%03X #5: 0x%03X #6: 0x%03X #7: 0x%03X Loop:        0x%03X\n"
			   "Space used by echo: 0x%04X bytes.  Space used by samples: 0x%04X bytes.\n\n",
			   tracks[0].data.size(), tracks[1].data.size(), tracks[2].data.size(), tracks[3].data.size(), spaceForPointersAndInstrs,
			   tracks[4].data.size(), tracks[5].data.size(), tracks[6].data.size(), tracks[7].data.size(), tracks[CHANNELS].data.size(),
			   echoBufferSize << 11, spaceUsedBySamples);
	}
	//}
	if (totalSize > minSize && minSize > 0)
		std::cout << "File " << name << ", line " << line << ": Warning: Song was larger than it could pad out by 0x" << hex4 << totalSize - minSize << " bytes." << std::dec << std::endl;

	std::stringstream statStrStream;

	for (int i = 0; i < CHANNELS; ++i)		// // //
		statStrStream << "CHANNEL " << ('0' + i) << " SIZE:				0x" << hex4 << tracks[i].data.size() << "\n";
	statStrStream << "LOOP DATA SIZE:				0x" << hex4 << tracks[CHANNELS].data.size() << "\n";
	statStrStream << "POINTERS AND INSTRUMENTS SIZE:		0x" << hex4 << spaceForPointersAndInstrs << "\n";
	statStrStream << "SAMPLES SIZE:				0x" << hex4 << spaceUsedBySamples << "\n";
	statStrStream << "ECHO SIZE:				0x" << hex4 << (echoBufferSize << 11) << "\n";

	int totalSize = spaceForPointersAndInstrs;		// // //
	std::for_each(std::cbegin(tracks), std::cend(tracks), [&] (const Track &x) { totalSize += x.data.size(); });
	statStrStream << "SONG TOTAL DATA SIZE:			0x" << hex4 << totalSize << "\n";

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

void Music::parseDefine() {
	error("A #define was found after the preprocessing stage.");
	//skipSpaces();
	//std::string defineName;
	//while (!isspace(peek()) && pos < text.length())
	//{
	//	defineName += text[pos++];
	//}

	//for (unsigned int z = 0; z < defineStrings.size(); z++)
	//	if (defineStrings[z] == defineName)
	//		error("A string cannot be defined more than once.");

	//defineStrings.push_back(defineName);
}

void Music::parseUndef() {
	error("An #undef was found after the preprocessing stage.");
	//	skipSpaces();
	//	std::string defineName;
	//	while (!isspace(peek()) && pos < text.length())
	//	{
	//		defineName += text[pos++];
	//	}
	//	unsigned int z = -1;
	//	for (z = 0; z < defineStrings.size(); z++)
	//		if (defineStrings[z] == defineName)
	//			goto found;
	//	
	//	error("The specified string was never defined.");
	//
	//found:
	//
	//	defineStrings[z].clear();
}

void Music::parseIfdef() {
	error("An #ifdef was found after the preprocessing stage.");
	//	inDefineBlock = true;
	//	skipSpaces();
	//	std::string defineName;
	//	while (!isspace(peek()) && pos < text.length())
	//	{
	//		defineName += text[pos++];
	//	}
	//
	//	unsigned int z = -1;
	//
	//	int temp;
	//
	//	for (unsigned int z = 0; z < defineStrings.size(); z++)
	//		if (defineStrings[z] == defineName)
	//			goto found;
	//
	//	temp = text.find("#endif", pos);
	//
	//	if (temp == -1)
	//		error("#ifdef was missing a matching #endif.");
	//
	//	pos = temp;
	//found:
	//	return;
}

void Music::parseIfndef() {
	error("An #ifndef was found after the preprocessing stage.");
	//	inDefineBlock = true;
	//	skipSpaces();
	//	std::string defineName;
	//	while (!isspace(peek()) && pos < text.length())
	//	{
	//		defineName += text[pos++];
	//	}
	//
	//	unsigned int z = -1;
	//
	//	for (unsigned int z = 0; z < defineStrings.size(); z++)
	//		if (defineStrings[z] == defineName)
	//			goto found;
	//
	//	return;
	//
	//found:
	//	int temp = text.find("#endif", pos);
	//
	//	if (temp == -1)
	//		error("#ifdef was missing a matching #endif.");
	//
	//	pos = temp;
}

void Music::parseEndif() {
	error("An #endif was found after the preprocessing stage.");
	//if (inDefineBlock == false)
	//	error("#endif was found without a matching #ifdef or #ifndef")
	//else
	//	inDefineBlock = false;
}

void Music::parseSPCInfo() {
	skipSpaces();
	if (!trimChar('{'))
		error("Could not find opening brace in SPC info command.");
	skipSpaces();

	while (!trimChar('}')) {
		if (!trimChar('#'))		// // //
			error("Unexpected symbol found in SPC info command.  Expected \'#\'.");
		std::string typeName = getIdentifier();		// // //

		if (typeName != "author" && typeName != "comment" && typeName != "title" && typeName != "game" && typeName != "length")
			error("Unexpected type name found in SPC info command.  Only \"author\", \"comment\", \"title\", \"game\", and \"length\" are allowed.");

		skipSpaces();
		if (!trimChar('\"'))
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
				int p = 0;
				int m = getInt(parameter, p);
				if (parameter[p] != ':' || m == -1)
					error("Error parsing SPC length field.  Format must be m:ss or \"auto\".");		// // //

				p++;
				int s = getInt(parameter, p);
				if (s == -1 || p != parameter.length())
					error("Error parsing SPC length field.  Format must be m:ss or \"auto\".");		// // //

				seconds = m * 60 + s;

				if (seconds > 999)
					error("Songs longer than 16:39 are not allowed by the SPC format.");		// // //

				seconds = seconds & 0xFFFFFF;
				knowsLength = true;
			}
		}

		skipSpaces();
	}

	if (author.length() > 32) {
		author = author.substr(0, 32);
		printWarning((std::string)("\"Author\" field was too long.  Truncating to \"") + author + "\".");
	}
	if (game.length() > 32) {
		game = game.substr(0, 32);
		printWarning((std::string)("\"Game\" field was too long.  Truncating to \"") + game + "\".");
	}
	if (comment.length() > 32) {
		comment = comment.substr(0, 32);
		printWarning((std::string)("\"Comment\" field was too long.  Truncating to \"") + comment + "\".");
	}
	if (title.length() > 32) {
		title = title.substr(0, 32);
		printWarning((std::string)("\"Title\" field was too long.  Truncating to \"") + title + "\".");
	}
}

void Music::handleNormalLoopEnter() {
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

void Music::handleSuperLoopEnter() {
	superLoopLength = 0;
	inE6Loop = true;
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
}

void Music::handleNormalLoopExit(int loopCount) {
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

void Music::handleSuperLoopExit(int loopCount) {
	inE6Loop = false;
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
}

void Music::handleNormalLoopRemoteCall(int loopCount) {
	if (loopLabel == 0)
		addNoteLength(normalLoopLength * loopCount);
	else
		addNoteLength(loopLengths[loopLabel] * loopCount);
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

int Music::divideByTempoRatio(int value, bool fractionIsError) {
	return value;
	/*
	int temp = value / tempoRatio;
	if (temp * tempoRatio != value)
		if (fractionIsError)
			printError("Using the tempo ratio on this value would result in a fractional value.", false, name, line);
		else
			printWarning("The tempo ratio resulted in a fractional value.", name, line);

	return temp;
	*/
}

int Music::multiplyByTempoRatio(int value) {
	int temp = value * tempoRatio;
	if (temp >= 256)
		printError("Using the tempo ratio on this value would cause it to overflow.", false, name, line);

	return temp;
}

// // //
const std::string &Music::getFileName() const {
	return name;
}
