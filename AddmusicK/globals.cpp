#include "globals.h"		// // //
#include <sstream>
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <map>
#include <stack>

//ROM rom;
std::vector<uint8_t> rom;		// // //

Music musics[256];
//Sample samples[256];
std::vector<Sample> samples;
SoundEffect soundEffectsDF9[256];
SoundEffect soundEffectsDFC[256];
SoundEffect *soundEffects[2] = {soundEffectsDF9, soundEffectsDFC};
//std::vector<SampleGroup> sampleGroups;
std::vector<BankDefine *> bankDefines;
std::map<fs::path, int> sampleToIndex;		// // //

bool convert = true;
bool checkEcho = true;
bool forceSPCGeneration = false;
int bankStart = 0x200000;
bool verbose = false;
bool aggressive = false;
bool dupCheck = true;
bool validateHex = true;
bool doNotPatch = false;
int errorCount = 0;
bool optimizeSampleUsage = true;
bool usingSA1 = false;
bool allowSA1 = true;
bool forceNoContinuePrompt = false;
bool sfxDump = false;
bool visualizeSongs = false;
bool redirectStandardStreams = false;

unsigned programPos;		// // //
unsigned programUploadPos;
unsigned mainLoopPos;
unsigned reuploadPos;
unsigned SRCNTableCodePos;
unsigned programSize;
int highestGlobalSong;
int totalSampleCount;
int songCount = 0;
int songSampleListSize;
bool useAsarDLL;



void openFile(const fs::path &fileName, std::vector<uint8_t> &v)		// // //
{
	std::ifstream is(fileName, std::ios::binary);
	if (!is)
		printError(std::string("Error: File \"") + fileName.string() + std::string("\" not found."), true);

	is.seekg(0, std::ios::end);
	unsigned int length = (unsigned int)is.tellg();
	is.seekg(0, std::ios::beg);
	v.clear();
	v.reserve(length);

	while (length > 0) {
		char temp;
		is.read(&temp, 1);
		v.push_back(temp);
		length--;
	}

	is.close();
}

void openTextFile(const fs::path &fileName, std::string &s) {
	std::ifstream is(fileName);

	if (!is)
		printError(std::string("Error: File \"") + fileName.string() + std::string("\" not found."), true);

	s.assign(std::istreambuf_iterator<char> {is}, std::istreambuf_iterator<char> { });
}

time_t getTimeStamp(const fs::path &file) {
	// // // TODO: <chrono>
	if (!fs::exists(file)) {
		//std::cerr << "Could not determine timestamp of \"" << file << "\"." << std::endl;
		return 0;
	}
	return fs::last_write_time(file).time_since_epoch().count();
}

void printError(const std::string &error, bool isFatal, const std::string &fileName, int line) {
	if (line >= 0)		// // //
		std::cerr << std::dec << "File: " << fileName << " Line: " << line << ":\n";
	++errorCount;
	std::cerr << error << '\n';
	if (isFatal)
		quit(1);
}

void printWarning(const std::string &error, const std::string &fileName, int line) {
	std::ostringstream oss;
	if (line >= 0)
		oss << "File: " << fileName << " Line: " << line << ":\n";
	puts((oss.str() + error).c_str());
	putchar('\n');
}

void quit(int code) {
#ifdef _DEBUG		// // //
	if (code != 0)
		_CrtDbgBreak();
#else
	if (forceNoContinuePrompt == false) {
		puts("Press ENTER to continue...\n");
		getc(stdin);
	}
#endif
	std::exit(code);
}

int execute(const std::string &command, bool prepend) {
	std::string tempstr = command;		// // //
	if (prepend) {
#ifndef _WIN32
		tempstr.insert(0, "./");
#endif
	}
	return system(tempstr.c_str());
}

int scanInt(const std::string &str, const std::string &value)		// Scans an integer value that comes after the specified string within another string.  Must be in $XXXX format (or $XXXXXX, etc.).
{
	int i, ret;
	if ((i = str.find(value)) == -1)
		printError(std::string("Error: Could not find \"") + value + "\"", true);

	std::sscanf(str.c_str() + i + value.length(), "$%X", &ret);	// Woo C functions in C++ code!
	return ret;
}

// // //

void removeFile(const fs::path &fileName) {
	if (fs::exists(fileName) && !fs::remove(fileName)) {		// // //
		std::cerr << "Could not delete critical file \"" << fileName << "\"." << std::endl;
		quit(1);
	}
}

void writeTextFile(const fs::path &fileName, const std::string &string) {
	std::ofstream ofs;
	ofs.open(fileName, std::ios::binary);

	std::string n = string;

#ifdef _WIN32
	unsigned int i = 0;
	while (i < n.length()) {
		if (n[i] == '\n') {
			n = n.insert(i, "\r");
			i++;
		}
		i++;
	}
#endif
	ofs.write(n.c_str(), n.size());

	ofs.close();
}

void insertValue(int value, int length, const std::string &find, std::string &str) {
	int pos = str.find(find);
	if (pos == -1) { std::cout << "Error: \"" << find << "\" could not be found." << std::endl; quit(1); }
	pos += find.length();

	std::stringstream ss;
	ss << hex_formatter(length) << value << std::dec;		// // //
	std::string tempStr = ss.str();
	str.replace(pos + 1, length, tempStr);
}

//int getSampleIndex(const std::string &name)
//{
//	for (int i = 0; i < 256; i++)
//		if (samples[i].exists)
//			if (name == samples[i].name) 
//				return i;
//
//	return -1;
//}

//void loadSample(const std::string &name, Sample *srcn)
//{
//	std::vector<byte> temp;
//
//
//	//unsigned char *temp;
//	openFile(std::string("samples/") + name, temp);
//	
//	srcn->name = name; //= (char *)calloc(strlen(name) + 1, 1);
//	//if (srcn->name == NULL) printError(OutOfMemory, true);
//	//strncpy(srcn->name, name, strlen(name));
//
//	if ((temp.size()) % 9 == 0) 
//	{
//		//srcn->data = temp;
//		srcn->data = temp;
//		srcn->size = temp.size();
//		//srcn->data = srcn;
//		for (int k = 0; (unsigned)k < temp.size(); k+=9)
//		{
//			if ((srcn->data[k] & 0x02) != 0x02) 
//				srcn->loopPoint = k; 
//			else 
//				break;
//		}
//	}
//	else 
//	{
//		srcn->size = temp.size() - 2;
//		srcn->loopPoint = temp[1] << 8 | temp[0];
//		srcn->data = temp;
//		srcn->data.erase(srcn->data.begin(), srcn->data.begin() + 2);
//		//srcn->data = alloc(dataSize - 2);
//		//memcpy(srcn->data, temp + 2, dataSize - 2);
//		//
//	}
//
//	srcn->exists = true;
//}

int findFreeSpace(unsigned int size, int start, std::vector<uint8_t> &ROM)		// // //
{
	if (size == 0)
		printError("Internal error: Requested free ROM space cannot be 0 bytes.", true);
	if (size > 0x7FF8)
		printError("Internal error: Requested free ROM space cannot exceed 0x7FF8 bytes.", true);

	size_t pos = 0;
	size_t runningSpace = 0;
	size += 8;
	int i;

	for (i = start; (unsigned)i < ROM.size(); i++) {
		if (runningSpace == size) {
			pos = i;
			break;
		}

		if (i % 0x8000 == 0)
			runningSpace = 0;

		if ((unsigned)i < ROM.size() - 4 && memcmp(&ROM[i], "STAR", 4) == 0) {
			unsigned short RATSSize = ROM[i + 4] | ROM[i + 5] << 8;
			unsigned short sizeInv = (ROM[i + 6] | ROM[i + 7] << 8) ^ 0xFFFF;
			if (RATSSize != sizeInv) {
				runningSpace += 1;
				continue;
			}
			i += RATSSize + 8;	// Would be nine if the loop didn't auto increment.
			runningSpace = 0;

		}
		else if (ROM[i] == 0 || aggressive) {
			runningSpace += 1;
		}
		else {
			runningSpace = 0;
		}
	}

	if (runningSpace == size)
		pos = i;

	if (pos == 0) {
		if (start == 0x080000)
			return -1;
		else
			return findFreeSpace(size, 0x080000, rom);
	}

	pos -= size;

	ROM[pos + 0] = 'S';
	ROM[pos + 1] = 'T';
	ROM[pos + 2] = 'A';
	ROM[pos + 3] = 'R';
	pos += 4;
	size -= 9;			// Not -8.  -8 would accidentally protect one too many bytes.
	ROM[pos + 0] = size & 0xFF;
	ROM[pos + 1] = size >> 8;
	size ^= 0xFFFF;
	ROM[pos + 2] = size & 0xFF;
	ROM[pos + 3] = size >> 8;
	pos -= 4;
	return pos;
}

int SNESToPC(int addr)					// Thanks to alcaro.
{
	if (addr < 0 || addr > 0xFFFFFF ||		// not 24bit 
		(addr & 0xFE0000) == 0x7E0000 ||	// wram 
		(addr & 0x408000) == 0x000000)		// hardware regs 
		return -1;
	if (usingSA1 && addr >= 0x808000)
		addr -= 0x400000;
	addr = ((addr & 0x7F0000) >> 1 | (addr & 0x7FFF));
	return addr;
}

int PCToSNES(int addr) {
	if (addr < 0 || addr >= 0x400000)
		return -1;

	addr = ((addr << 1) & 0x7F0000) | (addr & 0x7FFF) | 0x8000;

	if ((addr & 0xF00000) == 0x700000)
		addr |= 0x800000;

	if (usingSA1 && addr >= 0x400000)
		addr += 0x400000;
	return addr;
}

int clearRATS(int offset) {
	int size = ((rom[offset + 5] << 8) | rom[offset + 4]) + 8;
	int r = size;
	while (size >= 0)
		rom[offset + size--] = 0;
	return r + 1;
}

void addSample(const fs::path &fileName, Music *music, bool important) {
	std::vector<uint8_t> temp;		// // //
	fs::path actualPath = getSamplePath(fileName, music->name);
	openFile(actualPath, temp);
	addSample(temp, actualPath.string(), music, important, false);
}

// // //
void addSample(const std::vector<uint8_t> &sample, const std::string &name, Music *music, bool important, bool noLoopHeader, int loopPoint) {
	Sample newSample;
	if (important)
		newSample.important = true;
	else
		newSample.important = false;

	if (sample.size() != 0) {
		if (!noLoopHeader) {
			if ((sample.size() - 2) % 9 != 0) {
				std::stringstream errstream;

				errstream << "The sample \"" + name + "\" was of an invalid length (the filesize - 2 should be a multiple of 9).  Did you forget the loop header?" << std::endl;
				printError(errstream.str(), true);
			}

			newSample.loopPoint = (sample[1] << 8) | (sample[0]);
			newSample.data.assign(sample.begin() + 2, sample.end());
		}
		else {
			newSample.data.assign(sample.begin(), sample.end());
			newSample.loopPoint = loopPoint;
		}
	}
	newSample.exists = true;
	newSample.name = name;

	if (dupCheck) {
		for (size_t i = 0, n = samples.size(); i < n; ++i)		// // //
			if (samples[i].name == newSample.name) {
				music->mySamples.push_back(static_cast<uint16_t>(i));
				return;						// Don't add two of the same sample.
			}

		for (size_t i = 0, n = samples.size(); i < n; ++i)
			if (samples[i].data == newSample.data) {
				sampleToIndex[name] = i;
				music->mySamples.push_back(static_cast<uint16_t>(i));
				return;
			}
	}
	sampleToIndex[newSample.name] = samples.size();
	music->mySamples.push_back(static_cast<uint16_t>(samples.size()));		// // //
	samples.push_back(newSample);					// This is a sample we haven't encountered before.  Add it.
}

void addSampleGroup(const fs::path &groupName, Music *music) {
	for (const auto &bank : bankDefines) {		// // //
		if (groupName == bank->name) {		// // //
			for (size_t j = 0, n = bank->samples.size(); j < n; ++j) {
				std::string temp;
				//temp += "samples/";
				temp += *(bank->samples[j]);
				addSample(temp, music, bank->importants[j]);
			}
			return;
		}
	}
	std::cerr << music->name << ":\n";
	std::cerr << "The specified sample group, \"" << groupName << "\", could not be found." << std::endl;
	quit(1);
}

int bankSampleCount = 0;			// Used to give unique names to sample bank brrs.

void addSampleBank(const fs::path &fileName, Music *music) {
	std::vector<uint8_t> bankFile;		// // //

	fs::path actualPath = getSamplePath(fileName, music->name);		// // //
	openFile(actualPath, bankFile);

	if (bankFile.size() != 0x8000)
		printError("The specified bank file was an illegal size.", true);
	bankFile.erase(bankFile.begin(), bankFile.begin() + 12);
	//Sample bankSamples[0x40];
	Sample tempSample;
	int currentSample = 0;
	for (currentSample = 0; currentSample < 0x40; currentSample++) {
		unsigned short startPosition = bankFile[currentSample * 4 + 0] | (bankFile[currentSample * 4 + 1] << 8);
		tempSample.loopPoint = (bankFile[currentSample * 4 + 2] | bankFile[currentSample * 4 + 3] << 8) - startPosition;
		tempSample.data.clear();

		if (startPosition == 0 && tempSample.loopPoint == 0) {
			addSample("EMPTY.brr", music, true);
			continue;
		}

		startPosition -= 0x8000;

		size_t pos = startPosition;		// // //
		while (pos < bankFile.size()) {
			for (int i = 0; i <= CHANNELS; ++i)
				tempSample.data.push_back(bankFile[pos++]);
			if ((tempSample.data[tempSample.data.size() - 9] & 1) == 1)
				break;
		}

		char temp[20];
		sprintf(temp, "__SRCNBANKBRR%04X", bankSampleCount++);
		tempSample.name = temp;
		addSample(tempSample.data, tempSample.name, music, true, true, tempSample.loopPoint);
	}
}

int getSample(const fs::path &name, Music *music) {
	fs::path actualPath = getSamplePath(name, music->name);		// // //

	for (const auto &x : sampleToIndex) {
		fs::path p2 = x.first;
		if (fs::equivalent(actualPath, p2))
			return x.second;
	}

	return -1;
}

fs::path getSamplePath(const fs::path &name, const std::string &musicName) {
	fs::path actualPath;
	fs::path absoluteDir = "samples" / name;
	fs::path relativeDir = (fs::path("music") / musicName).parent_path();
	relativeDir.append(name);

	if (fs::exists(relativeDir))
		actualPath = relativeDir;
	else if (fs::exists(absoluteDir))
		actualPath = absoluteDir;
	else
		printError("Could not find sample " + name.string(), true, musicName);

	return actualPath;
}

// // //

#define error(str) printError(str, true, filename, line)

std::string getArgument(const std::string &str, char endChar, unsigned int &pos, bool breakOnNewLines) {		// // //
	std::string temp;

	while (true) {
		if (endChar == ' ') {
			if (str[pos] == ' ' || str[pos] == '\t')
				break;
		}
		else
			if (str[pos] == endChar)
				break;

		if (pos == str.length())
			printError("Unexpected end of file found.", true);		// // //
		if (breakOnNewLines) if (str[pos] == '\r' || str[pos] == '\n') break;			// Break on new lines.
		temp += str[pos];
		pos++;
	}

	return temp;
}

int strToInt(const std::string &str) {
	std::stringstream a;
	a << str;
	int j;
	a >> j;
	if (a.fail())
		throw std::invalid_argument("Could not parse string");
	return j;
}

#define skipSpaces(...) \
	while (i < str.size() && isspace(str[i]) && str[i] != '\n' && str[i] != '\r') \
		++i

// // //
PreprocessStatus preprocess(const std::string &str, const std::string &filename) {
	// Handles #ifdefs.  Maybe more later?
	PreprocessStatus stat;

	unsigned int i = 0;
	int level = 0, line = 1;

	std::map<std::string, int> defines;
	bool okayToAdd = true;
	std::stack<bool> okayStatus;

	if (str.substr(0, 3) == "\xEF\xBB\xBF")		// // // utf-8 bom
		i = 3;
	if (std::any_of(str.cbegin() + i, str.cend(), [] (char ch) { return static_cast<unsigned char>(ch) > 0x7Fu; }))
		printWarning("Non-ASCII characters detected.");

	while (true) {
		if (i == str.length()) break;

		if (i < str.length())
			if (str[i] == '\n')
				line++;

		if (str[i] == '\"') {
			i++;
			if (okayToAdd) {
				stat.result += '\"';
				stat.result += getArgument(str, '\"', i, false) + '\"';		// // //
			}
			i++;

		}
		else if (str[i] == '#') {
			std::string temp;
			i++;

			if (strncmp(str.c_str() + i, "amk=1", 5) == 0) {		// Special handling so that we can have #amk=1.
				if (stat.version >= 0)		// // //
					stat.version = 1;
				i += 5;
				continue;
			}

			temp = getArgument(str, ' ', i, true);		// // //

			if (temp == "define") {
				if (!okayToAdd) { level++; continue; }

				skipSpaces();		// // //
				std::string temp2 = getArgument(str, ' ', i, true);		// // //
				if (temp2.empty())
					error("#define was missing its argument.");

				skipSpaces();		// // //
				std::string temp3 = getArgument(str, ' ', i, true);		// // //
				if (temp3.empty())
					defines[temp2] = 1;
				else {
					try {
						int j = strToInt(temp3);
						defines[temp2] = j;		// // //
					}
					catch (...) {
						error("Could not parse integer for #define.");
					}
				}
			}
			else if (temp == "undef") {
				if (!okayToAdd) { level++; continue; }

				skipSpaces();		// // //
				std::string temp2 = getArgument(str, ' ', i, true);		// // //
				if (temp2.empty())
					error("#undef was missing its argument.");
				defines.erase(temp2);
			}
			else if (temp == "ifdef") {
				if (!okayToAdd) { level++; continue; }

				skipSpaces();		// // //
				std::string temp2 = getArgument(str, ' ', i, true);		// // //
				if (temp2.empty())
					error("#ifdef was missing its argument.");

				okayStatus.push(okayToAdd);
				okayToAdd = (defines.find(temp2) != defines.end());

				level++;
			}
			else if (temp == "ifndef") {
				if (!okayToAdd) { level++; continue; }

				skipSpaces();		// // //
				std::string temp2 = getArgument(str, ' ', i, true);		// // //
				if (temp2.empty())
					error("#ifndef was missing its argument.");

				okayStatus.push(okayToAdd);
				okayToAdd = (defines.find(temp2) == defines.end());

				level++;
			}
			else if (temp == "if") {
				if (!okayToAdd) { level++; continue; }

				skipSpaces();		// // //
				std::string temp2 = getArgument(str, ' ', i, true);		// // //
				if (temp2.length() == 0)
					error("#if was missing its first argument.");

				if (defines.find(temp2) == defines.end())
					error("First argument for #if was never defined.");

				skipSpaces();		// // //
				std::string temp3 = getArgument(str, ' ', i, true);		// // //
				if (temp3.length() == 0)
					error("#if was missing its comparison operator.");

				skipSpaces();		// // //
				std::string temp4 = getArgument(str, ' ', i, true);		// // //
				if (temp4.length() == 0)
					error("#if was missing its second argument.");

				okayStatus.push(okayToAdd);

				try {
					int j = strToInt(temp4);
					if (temp3 == "==")		// // //
						okayToAdd = (defines[temp2] == j);
					else if (temp3 == ">")
						okayToAdd = (defines[temp2] > j);
					else if (temp3 == "<")
						okayToAdd = (defines[temp2] < j);
					else if (temp3 == "!=")
						okayToAdd = (defines[temp2] != j);
					else if (temp3 == ">=")
						okayToAdd = (defines[temp2] >= j);
					else if (temp3 == "<=")
						okayToAdd = (defines[temp2] <= j);
					else
						error("Unknown operator for #if.");
				}
				catch (...) {
					error("Could not parse integer for #if.");
				}

				level++;
			}
			else if (temp == "endif") {
				if (level > 0) {
					level--;
					okayToAdd = okayStatus.top();
					okayStatus.pop();
				}
				else
					error("There was an #endif without a matching #ifdef, #ifndef, or #if.");
			}
			else if (temp == "amk") {
				if (stat.version >= 0) {
					skipSpaces();		// // //
					std::string temp = getArgument(str, ' ', i, true);		// // //
					if (temp.empty())
						printError("#amk must have an integer argument specifying the version.", false, filename, line);
					else {
						int j;
						try {
							j = strToInt(temp);
						}
						catch (...) {
							error("Could not parse integer for #amk.");
						}
						stat.version = j;
					}
				}
			}
			else if (temp == "amm")
				stat.version = -2;
			else if (temp == "am4")
				stat.version = -1;
			else if (okayToAdd) {
				if (temp.size() == 1) {		// // //
					char ch = temp.front();
					if (ch >= '0' && ch <= '7')
						stat.firstChannel = std::min(stat.firstChannel, ch - '0');
				}
				stat.result += "#";
				stat.result += temp;
			}
		}
		else {
			if (okayToAdd || str[i] == '\n')
				stat.result += str[i];
			i++;
		}
	}

	if (level != 0)
		error("There was an #ifdef, #ifndef, or #if without a matching #endif.");

	if (stat.version != -2) {		// For now, skip comment erasing for #amm songs.  #amk songs will follow suit in a later version.
		while (true) {
			int p = stat.result.find(';');
			if (p == std::string::npos)
				break;
			int finish = stat.result.find('\n', p);
			if (finish == std::string::npos)
				finish = stat.result.size();
			if (stat.result.substr(p, 7) == ";title=")
				stat.title = stat.result.substr(p + 7, finish - p - 7);
			stat.result.replace(p, finish - p, "");
		}
	}

	return stat;		// // //
}

bool asarCompileToBIN(const fs::path &patchName, const fs::path &binOutputFile, bool dieOnError) {
	removeFile("temp.log");
	removeFile("temp.txt");

	if (useAsarDLL) {
		int binlen = 0;
		int buflen = 0x10000;		// 0x10000 instead of 0x8000 because a few things related to sound effects are stored at 0x8000 at times.

		uint8_t *binOutput = (uint8_t *)malloc(buflen);		// // //

		asar_patch(patchName.string().c_str(), (char *)binOutput, buflen, &binlen);		// // //
		int count = 0, currentCount = 0;
		std::string printout;

		asar_getprints(&count);

		while (currentCount != count) {
			printout += asar_getprints(&count)[currentCount];
			printout += "\n";
			currentCount++;
		}
		if (count > 0)
			writeTextFile("temp.txt", printout);
///////////////////////////////////////////////////////////////////////////////
		count = 0; currentCount = 0;
		printout.clear();

		asar_geterrors(&count);

		while (currentCount != count) {
			printout += asar_geterrors(&count)[currentCount].fullerrdata + (std::string)"\n";
			currentCount++;
		}
		if (count > 0) {
			writeTextFile("temp.log", printout);
			free(binOutput);
			return false;
		}

		std::vector<uint8_t> v;		// // //
		v.assign(binOutput, binOutput + binlen);
		writeFile(binOutputFile, v);
		free(binOutput);
		return true;
	}
	else {
		remove(binOutputFile);
		std::string s = "asar " + patchName.string() + " " + binOutputFile.string() + " 2> temp.log > temp.txt";		// // //
		execute(s);
		if (dieOnError && fs::exists("temp.log") && fs::file_size("temp.log") != 0)
			return false;
		return true;
	}
}

bool asarPatchToROM(const fs::path &patchName, const fs::path &romName, bool dieOnError) {
	removeFile("temp.log");
	removeFile("temp.txt");

	if (useAsarDLL) {
		int binlen = 0;
		int buflen;

		std::vector<uint8_t> patchrom;		// // //
		openFile(romName, patchrom);
		buflen = patchrom.size();

		asar_patch(patchName.string().c_str(), (char *)patchrom.data(), buflen, &buflen);		// // //
		int count = 0, currentCount = 0;
		std::string printout;

		asar_getprints(&count);

		while (currentCount != count) {
			printout += asar_getprints(&count)[currentCount];
			printout += "\n";
			currentCount++;
		}
		if (count > 0)
			writeTextFile("temp.txt", printout);
///////////////////////////////////////////////////////////////////////////////
		count = 0; currentCount = 0;
		printout.clear();

		asar_geterrors(&count);

		while (currentCount != count) {
			printout += asar_geterrors(&count)[currentCount].fullerrdata + (std::string)"\n";
			currentCount++;
		}
		if (count > 0) {
			writeTextFile("temp.log", printout);
			return false;
		}

		writeFile(romName, patchrom);
		return true;
	}
	else {
		std::string s = "asar " + patchName.string() + " " + romName.string() + " 2> temp.log > temp.txt";
		execute(s);
		if (dieOnError && fs::exists("temp.log") && fs::file_size("temp.log") > 0)		// // //
			return false;
		return true;
	}
}
