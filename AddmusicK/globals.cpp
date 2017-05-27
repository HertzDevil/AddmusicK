#include "globals.h"		// // //
#include <sstream>
#include <iostream>
#include "asardll.h"		// // //
#include "SoundEffect.h"		// // //
#include "Music.h"		// // //

std::vector<Sample> samples;
//std::vector<SampleGroup> sampleGroups;
std::vector<BankDefine> bankDefines;		// // //
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
unsigned programSize;		// // //
int highestGlobalSong;
int totalSampleCount;
int songCount = 0;
int songSampleListSize;
bool useAsarDLL;



std::vector<uint8_t> openFile(const fs::path &fileName) {		// // //
	std::ifstream is(fileName, std::ios::binary);
	if (!is)
		fatalError("Error: File \"" + fileName.string() + "\" not found.");		// // //

	std::vector<uint8_t> buf;		// // //
	buf.reserve(static_cast<size_t>(fs::file_size(fileName)));
	buf.insert(buf.cend(), std::istreambuf_iterator<char> {is}, std::istreambuf_iterator<char> { });		// // //

	is.close();
	return buf;
}

std::string openTextFile(const fs::path &fileName) {		// // //
	std::ifstream is(fileName);
	if (!is)
		fatalError("Error: File \"" + fileName.string() + "\" not found.");		// // //
	return std::string(std::istreambuf_iterator<char> {is}, std::istreambuf_iterator<char> { });
}

time_t getTimeStamp(const fs::path &file) {
	// // // TODO: <chrono>
	if (!fs::exists(file)) {
		//std::cerr << "Could not determine timestamp of \"" << file << "\".\n";
		return 0;
	}
	return fs::last_write_time(file).time_since_epoch().count();
}

// // //
void printError(const std::string &error, const std::string &fileName, int line) {
	printWarning(error, fileName, line);		// // //
	++errorCount;
}

void printWarning(const std::string &error, const std::string &fileName, int line) {
	if (line >= 0)		// // //
		std::cerr << "File: " << fileName << " Line: " << std::dec << line << ":\n";
	std::cerr << error << '\n';
}

// // //
void fatalError(const std::string &error, const std::string &fileName, int line) {
	printWarning(error, fileName, line);		// // //
	quit(1);
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
	return std::system(tempstr.c_str());
}

// // //

void removeFile(const fs::path &fileName) {
	if (fs::exists(fileName) && !fs::remove(fileName))		// // //
		fatalError("Could not delete critical file \"" + fileName.string() + "\".");
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
	if (pos == -1)		// // //
		fatalError("Error: \"" + find + "\" could not be found.");
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

int findFreeSpace(unsigned int size, int start, std::vector<uint8_t> &ROM) {		// // //
	if (size == 0)
		fatalError("Internal error: Requested free ROM space cannot be 0 bytes.");		// // //
	if (size > 0x7FF8)
		fatalError("Internal error: Requested free ROM space cannot exceed 0x7FF8 bytes.");

	size_t pos = 0;
	size_t runningSpace = 0;
	size += 8;

	size_t i = start;
	for (const size_t N = ROM.size(); i < N; ) {		// // //
		if (i % 0x8000 == 0)
			runningSpace = 0;

		if (i < N - 4 && ROM[i] == 'S' && ROM[i + 1] == 'T' && ROM[i + 2] == 'A' && ROM[i + 3] == 'R') {
			unsigned short RATSSize = ROM[i + 4] | ROM[i + 5] << 8;
			unsigned short sizeInv = (ROM[i + 6] | ROM[i + 7] << 8) ^ 0xFFFF;
			if (RATSSize != sizeInv)
				++runningSpace;
			else {
				i += RATSSize + 8;	// Would be nine if the loop didn't auto increment.
				runningSpace = 0;
			}
		}
		else if (ROM[i] == 0 || aggressive)
			++runningSpace;
		else
			runningSpace = 0;

		++i;
		if (runningSpace == size) {
			pos = i;
			break;
		}
	}

	if (pos == 0)
		return start == 0x080000 ? -1 : findFreeSpace(size, 0x080000, ROM);		// // //

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

int clearRATS(std::vector<uint8_t> &ROM, int offset) {		// // //
	size_t size = ((ROM[offset + 5] << 8) | ROM[offset + 4]) + 9;		// // //
	std::fill(ROM.begin() + offset, ROM.begin() + offset + size, 0);
	return size;
}

void addSample(const fs::path &fileName, Music *music, bool important) {
	fs::path actualPath = getSamplePath(fileName, music->name);		// // //
	std::vector<uint8_t> temp = openFile(actualPath);
	addSample(temp, actualPath.string(), music, important, false);
}

// // //
void addSample(const std::vector<uint8_t> &sample, const std::string &name, Music *music, bool important, bool noLoopHeader, int loopPoint) {
	Sample newSample;
	newSample.important = important;		// // //

	if (sample.size() != 0) {
		if (!noLoopHeader) {
			if ((sample.size() - 2) % 9 != 0) {
				std::stringstream errstream;

				errstream << "The sample \"" + name + "\" was of an invalid length (the filesize - 2 should be a multiple of 9).  Did you forget the loop header?\n";
				fatalError(errstream.str());		// // //
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
		if (groupName == bank.name) {		// // //
			for (size_t j = 0, n = bank.samples.size(); j < n; ++j) {
				std::string temp;
				//temp += "samples/";
				temp += bank.samples[j];		// // //
				addSample(temp, music, bank.importants[j]);
			}
			return;
		}
	}
	
	std::stringstream ss;		// // //
	ss << music->name << ":\nThe specified sample group, \"" << groupName << "\", could not be found.";
	fatalError(ss.str());
}

void addSampleBank(const fs::path &fileName, Music *music) {
	std::vector<uint8_t> bankFile = openFile(getSamplePath(fileName, music->name));		// // //

	if (bankFile.size() != 0x8000)
		fatalError("The specified bank file was an illegal size.");		// // //
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

		std::stringstream ss;		// // //
		static int bankSampleCount = 0;		// // // Used to give unique names to sample bank brrs.
		ss << "__SRCNBANKBRR" << hex4 << bankSampleCount++;
		tempSample.name = ss.str();
		addSample(tempSample.data, tempSample.name, music, true, true, tempSample.loopPoint);
	}
}

int getSample(const fs::path &name, Music *music) {
	fs::path actualPath = getSamplePath(name, music->name);		// // //

	for (const auto &x : sampleToIndex)
		if (fs::equivalent(actualPath, x.first))
			return x.second;

	return -1;
}

// // //
fs::path getSamplePath(const fs::path &name, const std::string &musicName) {
	for (const auto &path : {
		(fs::path("music") / musicName).parent_path() / name,
		"samples" / name,
	})
		if (fs::exists(path))
			return path;

	fatalError("Could not find sample " + name.string(), musicName);
}

// // //
template <typename F, typename G>
static void asarOutput(F f, G g, const fs::path &filename) {
	int count = 0;
	const auto *msg = f(&count);
	if (!count)
		return;
	std::string out;
	for (int i = 0; i < count; ++i) {
		out += g(msg[i]);
		out += '\n';
	}
	writeTextFile(filename, out);
}

// // //
template <typename F>
static bool asarDoCompile(const fs::path &patchName, const fs::path &outputName, bool dieOnError, F maker) {
	removeFile("temp.log");
	removeFile("temp.txt");

	if (useAsarDLL) {
		std::vector<uint8_t> binOutput = maker();
		int binlen = 0;
		int buflen = binOutput.size();

		bool suc = asar_patch(patchName.string().c_str(), reinterpret_cast<char *>(binOutput.data()), buflen, &binlen);		// // //

		asarOutput(asar_getprints, [] (const char *str) { return str; }, "temp.txt");
		if (!suc) {
			asarOutput(asar_geterrors, [] (const errordata &err) { return err.fullerrdata; }, "temp.log");
			return false;
		}

		writeFile(outputName, binOutput);
		return true;
	}
	else {
		remove(outputName);
		execute("asar " + patchName.string() + " " + outputName.string() + " 2> temp.log > temp.txt");		// // //
		return !(dieOnError && fs::exists("temp.log") && fs::file_size("temp.log") > 0);
	}
}

bool asarCompileToBIN(const fs::path &patchName, const fs::path &outputName, bool dieOnError) {
	if (!useAsarDLL)
		remove(outputName);
	// 0x10000 instead of 0x8000 because a few things related to sound effects are stored at 0x8000 at times.
	return asarDoCompile(patchName, outputName, dieOnError, [] { return std::vector<uint8_t>(0x10000); });
}

bool asarPatchToROM(const fs::path &patchName, const fs::path &outputName, bool dieOnError) {
	return asarDoCompile(patchName, outputName, dieOnError, [&] { return openFile(outputName); });
}
