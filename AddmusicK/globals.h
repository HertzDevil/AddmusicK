#pragma once		// // //

#ifdef _WIN32
	#define _CRT_SECURE_NO_WARNINGS
#endif

//#if defined(linux) && !defined(stricmp)
//#error Please use -Dstrnicmp=strncasecmp on Unix-like systems.
//#endif



// Grab yer ducktape, folks!
#ifdef _WIN32
#define strnicmp _strnicmp
#else
#define stricmp strcasecmp
#define strnicmp strncasecmp
#endif

// // //
const int AMKVERSION = 1;
const int AMKMINOR = 0;
const int AMKREVISION = 3;

const int PARSER_VERSION = 2;			// Used to keep track of incompatible changes to the parser

const int DATA_VERSION = 0;				// Used to keep track of incompatible changes to any and all compiled data, either to the SNES or to the PC

// // //
//class ROM;
class Music;
class SoundEffect;
struct Sample;
class File;
struct SampleGroup;

//#include "ROM.h"
#include "Music.h"
#include "Sample.h"
#include "SoundEffect.h"
#include "SampleGroup.h"
#include "Directory.h"
#include "BankDefine.h"
#include <string>
#include <vector>
#include <fstream>
#include <map>
#include <cstdint>		// // //
#include <iomanip>		// // //
#include "Directory.h"
#include "asardll.h"


//extern ROM rom;
extern std::vector<uint8_t> rom;		// // //

extern Music musics[256];
//extern Sample samples[256];
extern std::vector<Sample> samples;
extern SoundEffect *soundEffects[2];	// soundEffects[2][256];
extern std::vector<BankDefine *> bankDefines;

extern std::map<File, int> sampleToIndex;

extern bool convert;
extern bool checkEcho;
extern bool forceSPCGeneration;
extern int bankStart;
extern bool verbose;
extern bool aggressive;
extern bool dupCheck;
extern bool validateHex;
extern bool doNotPatch;
extern int errorCount;
extern bool optimizeSampleUsage;
extern bool usingSA1;
extern bool allowSA1;
extern bool forceNoContinuePrompt;
extern bool sfxDump;
extern bool visualizeSongs;
extern bool redirectStandardStreams;

extern int programPos;
extern int programUploadPos;
extern int reuploadPos;
extern int mainLoopPos;
extern int SRCNTableCodePos;
extern int programSize;
extern int highestGlobalSong;
//extern int totalSampleCount;
extern int songCount;
extern int songSampleListSize;

extern bool useAsarDLL;

// Return true if an error occurred (if "dieOnError" is true).
bool asarCompileToBIN(const File &patchName, const File &binOutput, bool dieOnError = true);
bool asarPatchToROM(const File &patchName, const File &romName, bool dieOnError = true);

void openFile(const File &fileName, std::vector<uint8_t> &vector);		// // //
void openTextFile(const File &fileName, std::string &string);

// // //
class hex_formatter
{
	int width_;
public:
	explicit constexpr hex_formatter(int w) : width_(w) { }
	template <typename Char, typename Traits>
	friend std::basic_ostream<Char, Traits> &
	operator<<(std::basic_ostream<Char, Traits> &os, const hex_formatter &f) {
		os << std::setw(f.width_) << std::setfill('0') << std::uppercase << std::hex;
		return os;
	}
};

constexpr auto hex2 = hex_formatter(2);
constexpr auto hex4 = hex_formatter(4);
constexpr auto hex6 = hex_formatter(6);

template <typename T>
void writeFile(const File &fileName, const std::vector<T> &vector)
{
	std::ofstream ofs;
	ofs.open(fileName, std::ios::binary);
	ofs.write((const char *)vector.data(), vector.size() * sizeof(T));
	ofs.close();
}

void writeTextFile(const File &fileName, const std::string &string);
int execute(const File &command, bool prepentDotSlash = true);

void printError(const std::string &error, bool isFatal, const std::string &fileName = "", int line = -1);
void printWarning(const std::string &error, const std::string &fileName = "", int line = -1);

void quit(int code);

int scanInt(const std::string &str, const std::string &value);

bool fileExists(const File &fileName);

unsigned int getFileSize(const File &fileName);

void removeFile(const File &fileName);

//int getSampleIndex(const std::string &name);

//void loadSample(const std::string &name, Sample *srcn);

void insertValue(int value, int length, const std::string &find, std::string &str);

int findFreeSpace(unsigned int size, int start, std::vector<uint8_t> &ROM);	// // // Returns a position in the ROM with the specified amount of free space, starting at the specified position.  NOT using SNES addresses!  This function writes a RATS address at the position returned.

int SNESToPC(int addr);

int PCToSNES(int addr);

int clearRATS(int PCaddr);

void addSample(const File &fileName, Music *music, bool important);
void addSample(const std::vector<uint8_t> &sample, const std::string &name, Music *music, bool important, bool noLoopHeader, int loopPoint = 0);		// // //
void addSampleGroup(const File &fileName, Music *music);
void addSampleBank(const File &fileName, Music *music);

int getSample(const File &name, Music *music);

void preprocess(std::string &str, const std::string &filename, int &version, std::string &title);		// // //

int strToInt(const std::string &str);

time_t getTimeStamp(const File &file);
