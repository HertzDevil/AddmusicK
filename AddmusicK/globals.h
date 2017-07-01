#pragma once		// // //

// // //
const int AMKVERSION = 1;
const int AMKMINOR = 0;
const int AMKREVISION = 5;		// // //

const int PARSER_VERSION = 2;			// Used to keep track of incompatible changes to the parser

const int DATA_VERSION = 0;				// Used to keep track of incompatible changes to any and all compiled data, either to the SNES or to the PC

const int SFX_BANKS = 2;		// // //

// // //
class Music;
class SoundEffect;

//#include "ROM.h"
#include "Sample.h"
#include "BankDefine.h"
#include <string>
#include <vector>
#include <fstream>
#include <map>
#include <cstdint>		// // //
#include <iomanip>		// // //

#include "Utility/fs.h"		// // //



extern std::vector<Sample> samples;
extern std::vector<BankDefine> bankDefines;		// // //

extern std::map<fs::path, int> sampleToIndex;

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

extern unsigned programPos;		// // //
extern unsigned programUploadPos;
extern unsigned reuploadPos;
extern unsigned mainLoopPos;
extern unsigned SRCNTableCodePos;
extern unsigned programSize;
extern int highestGlobalSong;
//extern int totalSampleCount;
extern int songCount;
extern int songSampleListSize;

extern bool useAsarDLL;

// Return true if an error occurred (if "dieOnError" is true).
bool asarCompileToBIN(const fs::path &patchName, const fs::path &outputName, bool dieOnError = true);
bool asarPatchToROM(const fs::path &patchName, const fs::path &outputName, bool dieOnError = true);

// // //
class hex_formatter
{
	int width_;
public:
	explicit constexpr hex_formatter(int w) : width_(w) { }
	template <typename Char, typename Traits>
	friend std::basic_ostream<Char, Traits> &
	operator<<(std::basic_ostream<Char, Traits> &os, const hex_formatter &f) {
		return os << std::setw(f.width_) << std::setfill('0') << std::uppercase << std::hex;
	}
};

constexpr auto hex2 = hex_formatter(2);
constexpr auto hex4 = hex_formatter(4);
constexpr auto hex6 = hex_formatter(6);

std::vector<uint8_t> openFile(const fs::path &fileName);		// // //
template <typename T>
void writeFile(const fs::path &fileName, const std::vector<T> &vector) {		// // //
	std::ofstream ofs;
	ofs.open(fileName, std::ios::binary);
	ofs.write((const char *)vector.data(), vector.size() * sizeof(T));
	ofs.close();
}

std::string openTextFile(const fs::path &fileName);		// // //
template <typename F>
void writeTextFile(const fs::path &fileName, F &&f) {		// // //
	if (auto out = std::ofstream(fileName))
		out << f();
}

void removeFile(const fs::path &fileName);

int execute(const std::string &command, bool prepentDotSlash = true);

bool YesNo();		// // //

void printError(const std::string &error, const std::string &fileName = "", int line = -1);		// // //
void printWarning(const std::string &error, const std::string &fileName = "", int line = -1);
[[noreturn]] void fatalError(const std::string &error, const std::string &fileName = "", int line = -1);		// // //
[[noreturn]] void quit(int code);		// // //

// // //

//int getSampleIndex(const std::string &name);

//void loadSample(const std::string &name, Sample *srcn);

void insertValue(int value, int length, const std::string &find, std::string &str);

// Returns a position in the ROM with the specified amount of free space, starting at the specified position.
// NOT using SNES addresses!  This function writes a RATS address at the position returned.
int findFreeSpace(unsigned int size, int start, std::vector<uint8_t> &ROM);		// // //

int clearRATS(std::vector<uint8_t> &ROM, int PCaddr);		// // //

void addSample(const fs::path &fileName, Music *music, bool important);
void addSampleGroup(const fs::path &fileName, Music *music);
void addSampleBank(const fs::path &fileName, Music *music);

int getSample(const fs::path &name, Music *music);
fs::path getSamplePath(const fs::path &name, const std::string &musicName);		// // //

// // //

time_t getTimeStamp(const fs::path &file);

// // //
template <std::size_t N, typename ForwardIt, typename T>
void assign_val(ForwardIt it, T x) {
	for (std::size_t i = 0; i < N; ++i) {
		*it++ = static_cast<uint8_t>(x);
		x >>= 8;
	}
}

template <typename ForwardIt, typename T>
void assign_short(ForwardIt it, T x) {
	assign_val<sizeof(short)>(it, x);
}
