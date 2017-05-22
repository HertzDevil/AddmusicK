#include "globals.h"
#include <sstream>
#include <iostream>
#include <fstream>
#include <vector>
#include <iomanip>
// // //
#include "../AM405Remover/AM405Remover.h"
#include <cstdint>
#include "lodepng.h"
#include <thread>
#include "asardll.h"		// // //
#include "SoundEffect.h"		// // //


bool waitAtEnd = true;
fs::path ROMName;		// // //

std::vector<uint8_t> romHeader;		// // //

void cleanROM();
void tryToCleanSampleToolData();
void tryToCleanAM4Data();
void tryToCleanAMMData();

void assembleSNESDriver();
void assembleSPCDriver();
void loadMusicList();
void loadSampleList();
void loadSFXList();
void compileSFX();
void compileGlobalData();
void compileMusic();
void fixMusicPointers();
void generateSPCs();
void assembleSNESDriver2();
void generateMSC();
void cleanUpTempFiles();

void generatePNGs();

void checkMainTimeStamps();
bool recompileMain = true;

time_t mostRecentMainModification = 0;		// The most recent modification to any sound effect file, any global song file, any list file, or any asm file


bool justSPCsPlease = false;
std::vector<std::string> textFilesToCompile;

int main(int argc, char* argv[]) {
/*
	const std::locale loc {"en_US.UTF8"};		// // //
	std::locale::global(loc);
	std::cout.imbue(loc);
	std::cerr.imbue(loc);
*/
	std::clock_t startTime = clock();

	std::cout << "AddmusicK version " << AMKVERSION << "." << AMKMINOR << "." << AMKREVISION << " by Kipernal" << std::endl;
	std::cout << "Parser version " << PARSER_VERSION << std::endl << std::endl;
	std::cout << "Protip: Be sure to read the readme! If there's an error or something doesn't\nseem right, it may have your answer!\n\n" << std::endl;

	std::vector<std::string> arguments;

	if (fs::exists("Addmusic_options.txt")) {		// // //
		std::string optionsString = openTextFile("Addmusic_options.txt");		// // //
		unsigned int osPos = 0;
		while (osPos < optionsString.size()) {
			// This is probably a catastrophicly bad idea on several levels, but I don't have the time do redo this entire section of code.
			// AddmusicK 2.0: Now with actually good programming habits! (probably not)

			std::string opsubstr;
			if (optionsString.find('\n', osPos) == -1)
				opsubstr = optionsString.substr(osPos);
			else
				opsubstr = optionsString.substr(osPos, optionsString.find('\n', osPos) - osPos);

			arguments.push_back(opsubstr);
			osPos += opsubstr.size() + 1;
		}
	}
	else
		for (int i = 1; i < argc; i++)
			arguments.push_back(argv[i]);

	std::ofstream redirectOut, redirectErr;		// // //

	for (const auto &arg : arguments) {		// // //
		waitAtEnd = false;			// If the user entered a command line argument, chances are they don't need the "Press Any Key To Continue . . ." prompt.
		if (arg == "-c")
			convert = false;
		else if (arg == "-e")
			checkEcho = false;
		else if (arg == "-b")
			bankStart = 0x080000;
		else if (arg == "-v")
			verbose = true;
		else if (arg == "-a")
			aggressive = true;
		else if (arg == "-d")
			dupCheck = false;
		else if (arg == "-h")
			validateHex = false;
		else if (arg == "-p")
			doNotPatch = true;
		else if (arg == "-u")
			optimizeSampleUsage = false;
		else if (arg == "-s")
			allowSA1 = false;
		else if (arg == "-dumpsfx")
			sfxDump = true;
		else if (arg == "-visualize")
			visualizeSongs = true;
		else if (arg == "-g")
			forceSPCGeneration = true;
		else if (arg == "-noblock")
			forceNoContinuePrompt = true;
		else if (arg == "-streamredirect") {
			redirectStandardStreams = true;
			redirectOut.open("AddmusicK_stdout", std::ios::out | std::ios::trunc);		// // //
			redirectErr.open("AddmusicK_stderr", std::ios::out | std::ios::trunc);
			std::cout.rdbuf(redirectOut.rdbuf());
			std::cerr.rdbuf(redirectErr.rdbuf());
		}
		else if (arg == "-norom") {
			if (!ROMName.empty())		// // //
				fatalError("Error: -norom cannot be used after a filepath has already been used.\n"		// // //
						   "Input your text files /after/ the -norom option.");
			justSPCsPlease = true;
		}
		else if (ROMName.empty() && arg[0] != '-')
			if (!justSPCsPlease)
				ROMName = arg;
			else
				textFilesToCompile.push_back(arg);
		else {
			if (arg != "-?")
				std::cout << "Unknown argument \"%s\"." << arg << "\n\n";

			std::cout << "Options:\n"		// // //
						 "\t-e: Turn off echo buffer checking.\n"
						 "\t-c: Force off conversion from Addmusic 4.05 and AddmusicM\n"
						 "\t-b: Do not attempt to save music data in bank 0x40 and above.\n"
						 "\t-v: Turn verbosity on.  More information will be displayed using this.\n"
						 "\t-a: Make free space finding more aggressive.\n"
						 "\t-d: Turn off duplicate sample checking.\n"
						 "\t-h: Turn off hex command validation.\n"
						 "\t-p: Create a patch, but do not patch it to the ROM.\n"
						 "\t-norom: Only generate SPC files, no ROM required.\n"
						 "\t-?: Display this message.\n\n";

			if (arg != "-?")
				quit(1);
		}
	}

	if (justSPCsPlease == false) {
		if (ROMName.empty()) {		// // //
			std::cout << "Enter your ROM name: ";		// // //
			std::string temp;
			std::getline(std::cin, temp);
			ROMName = temp;
			puts("\n\n");
		}

		useAsarDLL = asar_init();		// // //

		fs::path smc = fs::path {ROMName} += ".smc";		// // //
		fs::path sfc = fs::path {ROMName} += ".sfc";
		if (fs::exists(smc) && fs::exists(sfc))
			fatalError("Error: Ambiguity detected; there were two ROMs with the specified name (one\n"
					   "with a .smc extension and one with a .sfc extension). Either delete one or\n"
					   "include the extension in your filename.");
		else if (fs::exists(smc))
			ROMName = smc;
		else if (fs::exists(sfc))
			ROMName = sfc;
		else if (!fs::exists(ROMName))
			fatalError("ROM not found.");
		rom = openFile(ROMName);		// // //

		tryToCleanAM4Data();
		tryToCleanAMMData();

		if (rom.size() % 0x8000 != 0) {
			romHeader.assign(rom.begin(), rom.begin() + 0x200);
			rom.erase(rom.begin(), rom.begin() + 0x200);
		}
		//rom.openFromFile(ROMName);

		if (rom.size() <= 0x80000)
			fatalError("Error: Your ROM is too small. Save a level in Lunar Magic or expand it with\n"
					   "Lunar Expand, then try again.");		// // //

		if (rom[SNESToPC(0xFFD5)] == 0x23 && allowSA1)
			usingSA1 = true;
		else
			usingSA1 = false;
		cleanROM();
	}

	loadSampleList();
	loadMusicList();
	loadSFXList();

	checkMainTimeStamps();

	assembleSNESDriver();		// We need this for the upload position, where the SPC file's PC starts.  Luckily, this function is very short.

	if (recompileMain || forceSPCGeneration) {
		assembleSPCDriver();
		compileSFX();
		compileGlobalData();
	}

	if (justSPCsPlease) {
		for (int i = highestGlobalSong + 1; i < 256; i++)
			musics[i].exists = false;

		for (int i = 0, n = textFilesToCompile.size(); i < n; ++i) {		// // //
			if (highestGlobalSong + i >= 256)
				fatalError("Error: The total number of requested music files to compile exceeded 255.");		// // //
			musics[highestGlobalSong + 1 + i].exists = true;
			// // //
			musics[highestGlobalSong + 1 + i].name = textFilesToCompile[i];
		}
	}

	compileMusic();
	fixMusicPointers();

	generateSPCs();

	if (visualizeSongs)
		generatePNGs();

	if (justSPCsPlease == false) {
		assembleSNESDriver2();
		generateMSC();
#ifndef _DEBUG
		cleanUpTempFiles();
#endif
	}

	std::cout << "\nSuccess!\n" << std::endl;

	std::clock_t endTime = clock();
	if (((endTime - startTime) / (float)CLOCKS_PER_SEC) - 1 < 0.02)
		std::cout << "Completed in 1 second." << std::endl;
	else
		std::cout << "Completed in " << std::dec << std::setprecision(2) << std::fixed << (endTime - startTime) / (float)CLOCKS_PER_SEC << " seconds." << std::endl;

	if (waitAtEnd)
		quit(0);
}

void displayNewUserMessage() {
#ifdef _WIN32
	system("cls");
#else
	system("clear");
#endif

	if (forceNoContinuePrompt == false) {
		std::cout << "This is a clean ROM you're using AMK on.";
		std::this_thread::sleep_for(std::chrono::milliseconds(1500));
		std::cout << "\n\nSo here's a message for new users.";
		std::this_thread::sleep_for(std::chrono::milliseconds(1500));
		std::cout << "\n\nIf there's some error you don't understand,";
		std::this_thread::sleep_for(std::chrono::milliseconds(1500));
		std::cout << "\nOr if something weird happens and you don't know why,";
		std::this_thread::sleep_for(std::chrono::milliseconds(1500));
		std::cout << "\n\n\nRead the whole ";
		std::this_thread::sleep_for(std::chrono::milliseconds(750));
		std::cout << "buggin' ";
		std::this_thread::sleep_for(std::chrono::milliseconds(750));
		std::cout << "ever ";
		std::this_thread::sleep_for(std::chrono::milliseconds(750));
		std::cout << "lovin' ";
		std::this_thread::sleep_for(std::chrono::milliseconds(750));
		std::cout << "README!";
		std::this_thread::sleep_for(std::chrono::milliseconds(1500));
		std::cout << "\n\n\nReally, it has answers to some of the most basic questions.";
		std::this_thread::sleep_for(std::chrono::milliseconds(2000));
		std::cout << "\nIf for no one else than yourself, read the readme first.";
		std::this_thread::sleep_for(std::chrono::milliseconds(2000));
		std::cout << "\nThat way you don't get chastised for asking something answered by it.";
		std::this_thread::sleep_for(std::chrono::milliseconds(2000));
		std::cout << "\nNot every possible answer is in there,";
		std::this_thread::sleep_for(std::chrono::milliseconds(1500));
		std::cout << "\nBut save yourself some time and at least make an effort.";
		std::this_thread::sleep_for(std::chrono::milliseconds(2000));
		std::cout << "\n\nDo we have a deal?";
		std::this_thread::sleep_for(std::chrono::milliseconds(2500));
		std::cout << "\nAlright. Cool. Now go out there and use/make awesome music.";
		std::this_thread::sleep_for(std::chrono::milliseconds(1500));
		std::cout << "\n\n(Power users: Use -noblock to skip this on future new ROMs.)";
		std::this_thread::sleep_for(std::chrono::milliseconds(1500));
	}
}

void cleanROM() {
	//tryToCleanAM4Data();
	//tryToCleanAMMData();
	tryToCleanSampleToolData();

	if (rom[0x70000] == 0x3E && rom[0x70001] == 0x0E) {	// If this is a "clean" ROM, then we don't need to do anything.
		//displayNewUserMessage();
		writeFile("asm/SNES/temp.sfc", rom);
		return;
	}
	else {
		//"New Super Mario World Sample Utility 2.0 by smkdan"

		std::string romprogramname;
		romprogramname += (char)*(rom.data() + SNESToPC(0x0E8000));
		romprogramname += (char)*(rom.data() + SNESToPC(0x0E8001));
		romprogramname += (char)*(rom.data() + SNESToPC(0x0E8002));
		romprogramname += (char)*(rom.data() + SNESToPC(0x0E8003));

		if (romprogramname != "@AMK") {
			std::stringstream s;
			s << "Error: The identifier for this ROM, \"" << romprogramname << "\", could not be identified. It should\n"
				 "be \"@AMK\". This either means that some other program has modified this area of\n"
				 "your ROM, or your ROM is corrupted. Continue? (Y or N)" << std::endl;
			std::cout << s.str();

			int c = '\0';
			while (c != 'y' && c != 'Y' && c != 'n' && c != 'N')
				c = getchar();

			if (c == 'n' || c == 'N')
				quit(1);
		}

		int romversion = *(unsigned char *)(rom.data() + SNESToPC(0x0E8004));

		if (romversion > DATA_VERSION) {
			std::cout << "WARNING: This ROM was modified using a newer version of AddmusicK." << std::endl;
			std::cout << "You can continue, but it is HIGHLY recommended that you upgrade AMK first." << std::endl;
			std::cout << "Continue anyway? (Y or N)" << std::endl;

			int c = '\0';
			while (c != 'y' && c != 'Y' && c != 'n' && c != 'N')
				c = getchar();

			if (c == 'n' || c == 'N')
				quit(1);
		}

		int address = SNESToPC(*(unsigned int *)(rom.data() + 0x70005) & 0xFFFFFF);	// Address, in this case, is the sample numbers list.
		clearRATS(address - 8);								// So erase it all.

		int baseAddress = SNESToPC(*(unsigned int *)(rom.data() + 0x70008));		// Address is now the address of the pointers to the songs and samples.
		bool erasingSamples = false;

		int i = 0;
		while (true) {
			address = *(unsigned int *)(rom.data() + baseAddress + i * 3) & 0xFFFFFF;
			if (address == 0xFFFFFF) {						// 0xFFFFFF indicates an end of pointers.
				if (erasingSamples == false)
					erasingSamples = true;
				else
					break;
			}
			else {
				if (address != 0)
					clearRATS(SNESToPC(address - 8));
			}

			i++;
		}
	}

	writeFile("asm/SNES/temp.sfc", rom);
}

void assembleSNESDriver() {
	programUploadPos = scanInt(openTextFile("asm/SNES/patch.asm"), "!DefARAMRet = ");		// // //
}

void assembleSPCDriver() {
	fs::remove("temp.log");		// // //
	fs::remove("asm/main.bin");

	programPos = scanInt(openTextFile("asm/main.asm"), "base ");		// // //
	if (verbose)
		std::cout << "Compiling main SPC program, pass 1." << std::endl;

	//execute("asar asm/main.asm asm/main.bin 2> temp.log > temp.txt");

	//if (fileExists("temp.log"))
	if (!asarCompileToBIN("asm/main.asm", "asm/main.bin"))
		fatalError("asar reported an error while assembling asm/main.asm. Refer to temp.log for\ndetails.");		// // //

	std::string temptxt = openTextFile("temp.txt");		// // //
	mainLoopPos = scanInt(temptxt, "MainLoopPos: ");
	reuploadPos = scanInt(temptxt, "ReuploadPos: ");
	SRCNTableCodePos = scanInt(temptxt, "SRCNTableCodePos: ");

	fs::remove("temp.log");

	programSize = static_cast<unsigned>(fs::file_size("asm/main.bin"));		// // //
}

void loadMusicList() {
	std::string musicFile = openTextFile("Addmusic_list.txt");		// // //
	if (musicFile.empty() || musicFile.back() != '\n')
		musicFile.push_back('\n');

	unsigned int i = 0;

	bool inGlobals = false;
	bool inLocals = false;
	bool gettingName = false;
	int index = -1;
	int shallowSongCount = 0;
	highestGlobalSong = -1;		// // //

	std::string tempName;

	while (i < musicFile.length()) {
		if (isspace(musicFile[i]) && !gettingName) {
			i++;
			continue;
		}

		if (strncmp(musicFile.c_str() + i, "Globals:", 8) == 0) {
			inGlobals = true;
			inLocals = false;
			i += 8;
			continue;
		}

		if (strncmp(musicFile.c_str() + i, "Locals:", 7) == 0) {
			inGlobals = false;
			inLocals = true;
			i += 7;
			continue;
		}

		if (!inGlobals && !inLocals)
			fatalError("Error: Could not find \"Globals:\" label in list.txt");		// // //

		if (index < 0) {
			if ('0' <= musicFile[i] && musicFile[i] <= '9') index = musicFile[i++] - '0';
			else if ('A' <= musicFile[i] && musicFile[i] <= 'F') index = musicFile[i++] - 'A' + 10;
			else if ('a' <= musicFile[i] && musicFile[i] <= 'f') index = musicFile[i++] - 'a' + 10;
			else fatalError("Invalid number in list.txt.");

			index <<= 4;

			if ('0' <= musicFile[i] && musicFile[i] <= '9') index |= musicFile[i++] - '0';
			else if ('A' <= musicFile[i] && musicFile[i] <= 'F') index |= musicFile[i++] - 'A' + 10;
			else if ('a' <= musicFile[i] && musicFile[i] <= 'f') index |= musicFile[i++] - 'a' + 10;
			else if (isspace(musicFile[i])) index >>= 4;
			else fatalError("Invalid number in list.txt.");

			if (!isspace(musicFile[i]))
				fatalError("Invalid number in list.txt.");
			if (inGlobals)
				highestGlobalSong = std::max(highestGlobalSong, index);
			if (inLocals)
				if (index <= highestGlobalSong)
					fatalError("Error: Local song numbers must be lower than the largest global song number.");		// // //
		}
		else {
			if (musicFile[i] == '\n' || musicFile[i] == '\r') {
				musics[index].name = tempName;
//				if (inLocals && !justSPCsPlease)
//					musics[index].text = openTextFile(fs::path("music") / tempName);		// // //
				musics[index].exists = true;
				index = -1;
				i++;
				shallowSongCount++;
				gettingName = false;
				tempName.clear();
				continue;

			}
			gettingName = true;
			tempName += musicFile[i++];
		}
	}

	if (verbose)
		std::cout << "Read in all " << shallowSongCount << " songs.\n";		// // //

	for (int i = 255; i >= 0; i--) {
		if (musics[i].exists) {
			songCount = i + 1;
			break;
		}
	}
}

void loadSampleList() {
	std::string str = openTextFile("Addmusic_sample groups.txt");		// // //

	std::string groupName;
	std::string tempName;

	unsigned int i = 0;
	bool gettingGroupName = false;
	bool gettingSampleName = false;

	while (i < str.length()) {
		if (gettingGroupName == false && gettingSampleName == false) {
			if (groupName.length() == 0) {
				if (isspace(str[i])) {
					i++;
					continue;
				}
				else if (str[i] == '#') {
					i++;
					gettingGroupName = true;
					groupName.clear();
					continue;
				}
			}
			else {
				if (isspace(str[i])) {
					i++;
					continue;
				}
				else if (str[i] == '{') {
					i++;
					gettingSampleName = true;
					continue;
				}
				else
					fatalError("Error parsing sample groups.txt.  Expected opening curly brace.");		// // //
			}
		}
		else if (gettingGroupName == true) {
			if (isspace(str[i])) {
				BankDefine *sg = new BankDefine;
				sg->name = groupName;
				bankDefines.push_back(sg);
				i++;
				gettingGroupName = false;
				continue;
			}
			else {
				groupName += str[i];
				i++;
				continue;
			}
		}
		else if (gettingSampleName) {
			if (tempName.length() > 0) {
				if (str[i] == '\"') {
					tempName.erase(tempName.begin(), tempName.begin() + 1);
					bankDefines[bankDefines.size() - 1]->samples.push_back(new std::string(tempName));
					bankDefines[bankDefines.size() - 1]->importants.push_back(false);
					tempName.clear();
					i++;
					continue;
				}
				else {
					tempName += str[i];
					i++;
				}
			}
			else {
				if (isspace(str[i])) {
					i++;
					continue;
				}
				else if (str[i] == '\"') {
					i++;
					tempName += ' ';
					continue;
				}
				else if (str[i] == '}') {
					gettingSampleName = false;
					gettingGroupName = false;
					groupName.clear();
					i++;
					continue;
				}
				else if (str[i] == '!') {
					if (bankDefines[bankDefines.size() - 1]->importants.size() == 0)
						fatalError("Error parsing Addmusic_sample groups.txt: Importance specifier ('!') must come\n"
								   "after asample declaration, not before it.");		// // //
					bankDefines[bankDefines.size() - 1]->importants[bankDefines[bankDefines.size() - 1]->importants.size() - 1] = true;
					i++;
				}
				else
					fatalError("Error parsing sample groups.txt.  Expected opening quote.");
			}
		}
	}

//	for (i = 0; (unsigned)i < bankDefines.size(); i++)
//	{
//		for (unsigned int j = 0; j < bankDefines[i]->samples.size(); j++)
//		{
//			for (int k = 0; k < samples.size(); k++)
//			{
//				if (samples[k].name == bankDefines[i]->samples[j]->c_str()) goto sampleExists;
//				//if (strcmp(samples[k].name, bankDefines[i]->samples[j]->c_str()) == 0) goto sampleExists;
//			}
//
//			loadSample(bankDefines[i]->samples[j]->c_str(), &samples[samples.size()]);
//			samples[samples.size()].exists = true;
//sampleExists:;
//		}
//	}
}

void loadSFXList() {		// Very similar to loadMusicList, but with a few differences.
	std::string str = openTextFile("Addmusic_sound effects.txt");		// // //

	if (str[str.length() - 1] != '\n')
		str += '\n';

	unsigned int i = 0;

	bool in1DF9 = false;
	bool in1DFC = false;
	bool gettingName = false;
	int index = -1;
	bool isPointer = false;
	bool doNotAdd0 = false;

	std::string tempName;

	int SFXCount = 0;

	while (i < str.length()) {
		if (isspace(str[i]) && !gettingName) {
			i++;
			continue;
		}

		if (strncmp(str.c_str() + i, "SFX1DF9:", 8) == 0) {
			in1DF9 = true;
			in1DFC = false;
			i += 8;
			continue;
		}

		if (strncmp(str.c_str() + i, "SFX1DFC:", 8) == 0) {
			in1DF9 = false;
			in1DFC = true;
			i += 8;
			continue;
		}

		if (!in1DF9 && !in1DFC)
			fatalError("Error: Could not find \"SFX1DF9:\" label in sound effects.txt");		// // //

		if (index < 0) {
			if ('0' <= str[i] && str[i] <= '9') index = str[i++] - '0';
			else if ('A' <= str[i] && str[i] <= 'F') index = str[i++] - 'A' + 10;
			else if ('a' <= str[i] && str[i] <= 'f') index = str[i++] - 'a' + 10;
			else fatalError("Invalid number in sound effects.txt.");

			index <<= 4;

			if ('0' <= str[i] && str[i] <= '9') index |= str[i++] - '0';
			else if ('A' <= str[i] && str[i] <= 'F') index |= str[i++] - 'A' + 10;
			else if ('a' <= str[i] && str[i] <= 'f') index |= str[i++] - 'a' + 10;
			else if (isspace(str[i])) index >>= 4;
			else fatalError("Invalid number in sound effects.txt.");

			if (!isspace(str[i]))
				fatalError("Invalid number in sound effects.txt.");
		}
		else {
			if (str[i] == '*' && tempName.length() == 0) {
				isPointer = true;
				i++;
			}
			else if (str[i] == '?' && tempName.length() == 0) {
				doNotAdd0 = true;
				i++;
			}
			else if (str[i] == '\n' || str[i] == '\r') {
				if (in1DF9) {
					if (!isPointer)
						soundEffects[0][index].name = tempName;
					else
						soundEffects[0][index].pointName = tempName;

					soundEffects[0][index].exists = true;

					if (doNotAdd0)
						soundEffects[0][index].add0 = false;
					else
						soundEffects[0][index].add0 = true;

					if (!isPointer)
						soundEffects[0][index].text = openTextFile(fs::path("1DF9") / tempName);		// // //
				}
				else {
					if (!isPointer)
						soundEffects[1][index].name = tempName;
					else
						soundEffects[1][index].pointName = tempName;

					soundEffects[1][index].exists = true;

					if (doNotAdd0)
						soundEffects[1][index].add0 = false;
					else
						soundEffects[1][index].add0 = true;

					if (!isPointer)
						soundEffects[1][index].text = openTextFile(fs::path("1DFC") / tempName);		// // //
				}

				index = -1;
				i++;
				SFXCount++;
				gettingName = false;
				tempName.clear();
				isPointer = false;
				doNotAdd0 = false;
				continue;
			}
			else {
				gettingName = true;
				tempName += str[i++];
			}
		}
	}

	if (verbose)
		std::cout << "Read in all " << SFXCount << " sound effects.\n";		// // //
}

void compileSFX() {
	for (int i = 0; i < 2; i++) {
		for (int j = 1; j < 256; j++) {
			soundEffects[i][j].bank = i;
			soundEffects[i][j].index = j;
			if (soundEffects[i][j].pointName.length() > 0) {
				for (int k = 1; k < 256; k++) {
					if (soundEffects[i][j].pointName == soundEffects[i][k].name) {
						soundEffects[i][j].pointsTo = k;
						break;
					}
				}
				if (soundEffects[i][j].pointsTo == 0) {
					std::ostringstream r;
					r << "Error: The sound effect that sound effect 0x" << std::hex << j << " points to could not be found.";
					fatalError(r.str());		// // //
				}
			}
		}
	}
}

void compileGlobalData() {
	int DF9DataTotal = 0;
	int DFCDataTotal = 0;
	int DF9Count = 0;
	int DFCCount = 0;

	std::ostringstream oss;

	std::vector<unsigned short> DF9Pointers, DFCPointers;

	for (int i = 255; i >= 0; i--) {
		if (soundEffects[0][i].exists) {
			DF9Count = i;
			break;
		}
	}

	for (int i = 255; i >= 0; i--) {
		if (soundEffects[1][i].exists) {
			DFCCount = i;
			break;
		}
	}

	for (int i = 0; i <= DF9Count; i++) {
		if (soundEffects[0][i].exists && soundEffects[0][i].pointsTo == 0) {
			soundEffects[0][i].posInARAM = DFCCount * 2 + DF9Count * 2 + programPos + programSize + DF9DataTotal;
			soundEffects[0][i].compile();
			DF9Pointers.push_back(DF9DataTotal + (DF9Count + DFCCount) * 2 + programSize + programPos);
			DF9DataTotal += soundEffects[0][i].data.size() + soundEffects[0][i].code.size();
		}
		else if (soundEffects[0][i].exists == false)
			DF9Pointers.push_back(0xFFFF);
		else if (i > soundEffects[0][i].pointsTo)
			DF9Pointers.push_back(DF9Pointers[soundEffects[0][i].pointsTo]);
		else
			fatalError("Error: A sound effect that is a pointer to another sound effect must come after\n"
					   "the sound effect that it points to.");
	}

	if (errorCount > 0)
		fatalError("There were errors when compiling the sound effects.  Compilation aborted.  Your\n"
				   "ROM has not been modified.");

	for (int i = 0; i <= DFCCount; i++) {
		if (soundEffects[1][i].exists && soundEffects[1][i].pointsTo == 0) {
			soundEffects[1][i].posInARAM = DFCCount * 2 + DF9Count * 2 + programPos + programSize + DF9DataTotal + DFCDataTotal;
			soundEffects[1][i].compile();
			DFCPointers.push_back(DFCDataTotal + DF9DataTotal + (DF9Count + DFCCount) * 2 + programSize + programPos);
			DFCDataTotal += soundEffects[1][i].data.size() + soundEffects[1][i].code.size();
		}
		else if (!soundEffects[1][i].exists)
			DFCPointers.push_back(0xFFFF);
		else if (i > soundEffects[1][i].pointsTo)
			DFCPointers.push_back(DFCPointers[soundEffects[1][i].pointsTo]);
		else
			fatalError("Error: A sound effect that is a pointer to another sound effect must come after\n"
					   "the sound effect that it points to.");
	}

	if (errorCount > 0)
		fatalError("There were errors when compiling the sound effects.  Compilation aborted.  Your\n"
				   "ROM has not been modified.");

	if (verbose) {
		std::cout << "Total space used by 1DF9 sound effects: 0x" << hex4 << (DF9DataTotal + DF9Count * 2) << std::dec << std::endl;
		std::cout << "Total space used by 1DFC sound effects: 0x" << hex4 << (DFCDataTotal + DFCCount * 2) << std::dec << std::endl;
	}

	std::cout << "Total space used by all sound effects: 0x" << hex4 << (DF9DataTotal + DF9Count * 2 + DFCDataTotal + DFCCount * 2) << std::dec << std::endl;

	DF9Pointers.erase(DF9Pointers.begin(), DF9Pointers.begin() + 1);
	DFCPointers.erase(DFCPointers.begin(), DFCPointers.begin() + 1);

	writeFile("asm/SFX1DF9Table.bin", DF9Pointers);
	writeFile("asm/SFX1DFCTable.bin", DFCPointers);


	std::vector<uint8_t> allSFXData;		// // //

	for (int i = 0; i <= DF9Count; i++) {
		for (unsigned int j = 0; j < soundEffects[0][i].data.size(); j++)
			allSFXData.push_back(soundEffects[0][i].data[j]);
		for (unsigned int j = 0; j < soundEffects[0][i].code.size(); j++)
			allSFXData.push_back(soundEffects[0][i].code[j]);
	}

	for (int i = 0; i <= DFCCount; i++) {
		for (unsigned int j = 0; j < soundEffects[1][i].data.size(); j++)
			allSFXData.push_back(soundEffects[1][i].data[j]);
		for (unsigned int j = 0; j < soundEffects[1][i].code.size(); j++)
			allSFXData.push_back(soundEffects[1][i].code[j]);
	}

	writeFile("asm/SFXData.bin", allSFXData);

	std::string str = openTextFile("asm/main.asm");		// // //

	int pos;
	pos = str.find("SFXTable0:");
	if (pos == -1)
		fatalError("Error: SFXTable0 not found in main.asm.");
	str.insert(pos + 10, "\r\nincbin \"SFX1DF9Table.bin\"\r\n");

	pos = str.find("SFXTable1:");
	if (pos == -1)
		fatalError("Error: SFXTable1 not found in main.asm.");
	str.insert(pos + 10, "\r\nincbin \"SFX1DFCTable.bin\"\r\nincbin \"SFXData.bin\"\r\n");

	writeTextFile("asm/tempmain.asm", str);

	fs::remove("asm/main.bin");		// // //

	if (verbose)
		std::cout << "Compiling main SPC program, pass 2." << std::endl;

	//execute("asar asm/tempmain.asm asm/main.bin 2> temp.log > temp.txt");
	//if (fileExists("temp.log")) 
	if (!asarCompileToBIN("asm/tempmain.asm", "asm/main.bin"))
		fatalError("asar reported an error while assembling asm/main.asm. Refer to temp.log for\ndetails.\n");		// // //

	programSize = static_cast<unsigned>(fs::file_size("asm/main.bin"));		// // //

	std::cout << "Total size of main program + all sound effects: 0x" << hex4 << programSize << std::dec << std::endl;
}

void compileMusic() {
	if (verbose)
		std::cout << "Compiling music..." << std::endl;

	int totalSamplecount = 0;
	int totalSize = 0;
	for (int i = 0; i < 256; i++) {
		if (musics[i].exists) {
			if (!(i <= highestGlobalSong && !recompileMain)) {
				musics[i].index = i;
				musics[i].compile();
				totalSamplecount += musics[i].mySamples.size();
			}
		}
	}

	//int songSampleListSize = 0;

	//for (int i = 0; i < songCount; i++)
	//{
	//	songSampleListSize += musics[i].mySampleCount + 1;
	//}

	std::stringstream songSampleList;
	std::string s;

	songSampleListSize = 8;

	songSampleList << "db $53, $54, $41, $52\t\t\t\t; Needed to stop Asar from treating this like an xkas patch.\n";
	songSampleList << "dw SGEnd-SampleGroupPtrs-$01\ndw SGEnd-SampleGroupPtrs-$01^$FFFF\nSampleGroupPtrs:\n\n";

	for (int i = 0; i < songCount; i++) {
		if (i % 16 == 0)
			songSampleList << "\ndw ";
		if (musics[i].exists == false)
			songSampleList << "$" << hex4 << 0;
		else
			songSampleList << "SGPointer" << hex2 << i;
		songSampleListSize += 2;

		if (i != songCount - 1 && (i & 0xF) != 0xF)
			songSampleList << ", ";
		//s = songSampleList.str();
	}

	songSampleList << "\n\n";

	for (int i = 0; i < songCount; i++) {
		if (!musics[i].exists) continue;

		songSampleListSize++;

		songSampleList << "\n" << "SGPointer" << hex2 << i << ":\n";

		if (i > highestGlobalSong) {
			songSampleList << "db $" << hex2 << musics[i].mySamples.size() << "\ndw";
			for (unsigned int j = 0; j < musics[i].mySamples.size(); j++) {
				songSampleListSize += 2;
				songSampleList << " $" << hex4 << (int)(musics[i].mySamples[j]);
				if (j != musics[i].mySamples.size() - 1)
					songSampleList << ",";
			}
		}
	}

	songSampleList << "\nSGEnd:";
	s = songSampleList.str();
	std::stringstream tempstream;

	tempstream << "org $" << hex6 << PCToSNES(findFreeSpace(songSampleListSize, bankStart, rom)) << "\n\n" << std::endl;

	s.insert(0, tempstream.str());

	writeTextFile("asm/SNES/SongSampleList.asm", s);
}

void fixMusicPointers() {
	if (verbose)
		std::cout << "Fixing song pointers..." << std::endl;

	int pointersPos = programSize + 0x400;
	std::stringstream globalPointers;
	std::stringstream incbins;

	int songDataARAMPos = programSize + programPos + highestGlobalSong * 2 + 2;
	//                    size + startPos + pointer to each global song + pointer to local song.
	//int songPointerARAMPos = programSize + programPos;

	bool addedLocalPtr = false;

	for (int i = 0; i < 256; i++) {
		if (musics[i].exists == false) continue;

		musics[i].posInARAM = songDataARAMPos;

		int untilJump = -1;

		if (i <= highestGlobalSong) {
			globalPointers << "\ndw song" << hex2 << i;
			incbins << "song" << hex2 << i << ": incbin \"" << "SNES/bin/" << "music" << hex2 << i << ".bin\"\n";
		}
		else if (addedLocalPtr == false) {
			globalPointers << "\ndw localSong";
			incbins << "localSong: ";
			addedLocalPtr = true;
		}

		for (int j = 0; j < musics[i].spaceForPointersAndInstrs; j += 2) {
			if (untilJump == 0) {
				j += musics[i].instrumentData.size();
				untilJump = -1;
			}

			int temp = musics[i].allPointersAndInstrs[j] | musics[i].allPointersAndInstrs[j + 1] << 8;

			if (temp == 0xFFFF) {		// 0xFFFF = swap with 0x0000.
				musics[i].allPointersAndInstrs[j] = 0;
				musics[i].allPointersAndInstrs[j + 1] = 0;
				untilJump = 1;
			}
			else if (temp == 0xFFFE) {	// 0xFFFE = swap with 0x00FF.
				musics[i].allPointersAndInstrs[j] = 0xFF;
				musics[i].allPointersAndInstrs[j + 1] = 0;
				untilJump = 2;
			}
			else if (temp == 0xFFFD) {	// 0xFFFD = swap with the song's position (its first track pointer).
				musics[i].allPointersAndInstrs[j] = (musics[i].posInARAM + 2) & 0xFF;
				musics[i].allPointersAndInstrs[j + 1] = (musics[i].posInARAM + 2) >> 8;
			}
			else if (temp == 0xFFFC) {	// 0xFFFC = swap with the song's position + 2 (its second track pointer).
				musics[i].allPointersAndInstrs[j] = musics[i].posInARAM & 0xFF;
				musics[i].allPointersAndInstrs[j + 1] = musics[i].posInARAM >> 8;
			}
			else if (temp == 0xFFFB) {	// 0xFFFB = swap with 0x0000, but don't set untilSkip.
				musics[i].allPointersAndInstrs[j] = 0;
				musics[i].allPointersAndInstrs[j + 1] = 0;
			}
			else {
				temp += musics[i].posInARAM;
				musics[i].allPointersAndInstrs[j] = temp & 0xFF;
				musics[i].allPointersAndInstrs[j + 1] = temp >> 8;
			}
			untilJump--;
		}

		musics[i].adjustLoopPointers();		// // //

		std::vector<uint8_t> final;		// // //

		int sizeWithPadding = (musics[i].minSize > 0) ? musics[i].minSize : musics[i].totalSize;

		if (i > highestGlobalSong) {
			int RATSSize = musics[i].totalSize + 4 - 1;
			final.push_back('S');
			final.push_back('T');
			final.push_back('A');
			final.push_back('R');

			final.push_back(RATSSize & 0xFF);
			final.push_back(RATSSize >> 8);

			final.push_back(~RATSSize & 0xFF);
			final.push_back(~RATSSize >> 8);

			final.push_back(sizeWithPadding & 0xFF);
			final.push_back(sizeWithPadding >> 8);

			final.push_back(songDataARAMPos & 0xFF);
			final.push_back(songDataARAMPos >> 8);
		}

		musics[i].FlushSongData(final);		// // //

		if (musics[i].minSize > 0 && i <= highestGlobalSong)
			while (final.size() < musics[i].minSize)
				final.push_back(0);

		if (i > highestGlobalSong) {
			musics[i].finalData.resize(final.size() - 12);
			musics[i].finalData.assign(final.begin() + 12, final.end());
		}

		std::stringstream fname;
		fname << "asm/SNES/bin/music" << hex2 << i << ".bin";
		writeFile(fname.str(), final);

		if (i <= highestGlobalSong) {
			songDataARAMPos += sizeWithPadding;
		}
		else {
			if (checkEcho) {
				musics[i].spaceInfo.songStartPos = songDataARAMPos;
				musics[i].spaceInfo.songEndPos = musics[i].spaceInfo.songStartPos + sizeWithPadding;

				int checkPos = songDataARAMPos + sizeWithPadding;
				if ((checkPos & 0xFF) != 0) checkPos = ((checkPos >> 8) + 1) << 8;

				musics[i].spaceInfo.sampleTableStartPos = checkPos;

				checkPos += musics[i].mySamples.size() * 4;

				musics[i].spaceInfo.sampleTableEndPos = checkPos;

				int importantSampleCount = 0;
				for (unsigned int j = 0; j < musics[i].mySamples.size(); j++) {
					auto thisSample = musics[i].mySamples[j];
					auto thisSampleSize = samples[thisSample].data.size();
					bool sampleIsImportant = samples[thisSample].important;
					if (sampleIsImportant) importantSampleCount++;

					musics[i].spaceInfo.individualSampleStartPositions.push_back(checkPos);
					musics[i].spaceInfo.individualSampleEndPositions.push_back(checkPos + thisSampleSize);
					musics[i].spaceInfo.individialSampleIsImportant.push_back(sampleIsImportant);

					checkPos += thisSampleSize;
				}
				musics[i].spaceInfo.importantSampleCount = importantSampleCount;

				if ((checkPos & 0xFF) != 0) checkPos = ((checkPos >> 8) + 1) << 8;

				//musics[i].spaceInfo.echoBufferStartPos = checkPos;

				checkPos += musics[i].echoBufferSize << 11;

				//musics[i].spaceInfo.echoBufferEndPos = checkPos;

				musics[i].spaceInfo.echoBufferEndPos = 0x10000;
				if (musics[i].echoBufferSize > 0) {
					musics[i].spaceInfo.echoBufferStartPos = 0x10000 - (musics[i].echoBufferSize << 11);
					musics[i].spaceInfo.echoBufferEndPos = 0x10000;
				}
				else {
					musics[i].spaceInfo.echoBufferStartPos = 0xFF00;
					musics[i].spaceInfo.echoBufferEndPos = 0xFF04;
				}

				if (checkPos > 0x10000) {
					std::stringstream ss;		// // //
					ss << musics[i].getFileName() << ":\nEcho buffer exceeded total space in ARAM by 0x" <<
						hex4 << checkPos - 0x10000 << " bytes.";
					fatalError(ss.str());
				}
			}
		}
	}

	if (recompileMain) {
		std::string patch = openTextFile("asm/tempmain.asm");		// // //

		patch += globalPointers.str() + "\n" + incbins.str();

		writeTextFile("asm/tempmain.asm", patch);

		if (verbose)
			std::cout << "Compiling main SPC program, final pass." << std::endl;

		//removeFile("asm/SNES/bin/main.bin");

		//execute("asar asm/tempmain.asm asm/SNES/bin/main.bin 2> temp.log > temp.txt");

		//if (fileExists("temp.log"))
		if (!asarCompileToBIN("asm/tempmain.asm", "asm/SNES/bin/main.bin"))
			fatalError("asar reported an error while assembling asm/main.asm. Refer to temp.log for\ndetails.\n");		// // //
	}

	std::vector<uint8_t> temp = openFile("asm/SNES/bin/main.bin");		// // //
	programSize = static_cast<unsigned>(fs::file_size("asm/SNES/bin/main.bin"));		// // //
	std::vector<uint8_t> temp2;
	temp2.resize(temp.size() + 4);
	temp2[0] = programSize & 0xFF;
	temp2[1] = programSize >> 8;
	temp2[2] = programPos & 0xFF;
	temp2[3] = programPos >> 8;
	for (unsigned int i = 0; i < temp.size(); i++)
		temp2[4 + i] = temp[i];
	writeFile("asm/SNES/bin/main.bin", temp2);

	if (verbose)
		std::cout << "Total space in ARAM left for local songs: 0x" << hex4 << (0x10000 - programSize - 0x400) << " bytes." << std::dec << std::endl;

	int defaultIndex = -1, optimizedIndex = -1;
	for (unsigned int i = 0; i < bankDefines.size(); i++) {
		if (bankDefines[i]->name == "default")
			defaultIndex = i;
		if (bankDefines[i]->name == "optimized")
			optimizedIndex = i;
	}

	// Can't do this now since we can't get a sample correctly outside of a song.

	/*
	if (defaultIndex != -1)
	{
		int groupSize = 0;
		for (unsigned int i = 0; i < bankDefines[defaultIndex]->samples.size(); i++)
		{
			int j = getSample(File(*(bankDefines[defaultIndex]->samples[i])));
			if (j == -1) goto end1;
			groupSize += samples[j].data.size() + 4;
		}

		std::cout << "Total space in ARAM for local songs using #default: 0x" << hex4 << (0x10000 - programSize - 0x400 - groupSize) << " bytes." << std::dec << std::endl;
	}
end1:

	if (optimizedIndex != -1)
	{
		int groupSize = 0;
		for (unsigned int i = 0; i < bankDefines[optimizedIndex]->samples.size(); i++)
		{
			int j = getSample(File(*(bankDefines[optimizedIndex]->samples[i])));
			if (j == -1) goto end2;
			groupSize += samples[j].data.size() + 4;
		}

		std::cout << "Total space in ARAM for local songs using #optimized: 0x" << hex4 << (0x10000 - programSize - 0x400 - groupSize) << " bytes." << std::dec << std::endl;
	}
end2:;
	*/
}

void generateSPCs() {
	if (checkEcho == false)		// If echo buffer checking is off, then the overflow may be due to too many samples.
		return;			// In this case, trying to generate an SPC would crash.
	//byte base[0x10000];

	std::vector<uint8_t> programData = openFile("asm/SNES/bin/main.bin");		// // //
	programData.erase(programData.begin(), programData.begin() + 4);	// Erase the upload data.

	unsigned int localPos = programData.size() + programPos;

	//for (i = 0; i < programPos; i++) base[i] = 0;

	//for (i = 0; i < programSize; i++)
	//	base[i + programPos] = programData[i];

	enum : size_t
	{
		PC = 0x25,
		TITLE = 0x2E,
		GAME = 0x4E,
		DUMPER = 0x6E,
		COMMENT = 0x7E,
		DATE = 0x9E,
		LENGTH = 0xA9,
		FADEOUT = 0xAC,
		AUTHOR = 0xB1,
		RAM = 0x100,
		DSP_ADDR = RAM + 0x10000,
		SPC_SIZE = DSP_ADDR + 0x100,
	};

	std::vector<uint8_t> SPCBase = openFile("asm/SNES/SPCBase.bin");
	std::vector<uint8_t> DSPBase = openFile("asm/SNES/SPCDSPBase.bin");
	SPCBase.resize(SPC_SIZE);

	int SPCsGenerated = 0;

	enum spc_mode_t {MUSIC, SFX1, SFX2};		// // //

	const auto makeSPCfn = [&] (int i, const spc_mode_t mode, bool yoshi) {		// // //
		std::vector<uint8_t> SPC = SPCBase;		// // //

		if (mode == MUSIC) {		// // //
			std::copy_n(musics[i].title.cbegin(), std::min(32u, musics[i].title.size()), SPC.begin() + TITLE);
			std::copy_n(musics[i].game.cbegin(), std::min(32u, musics[i].game.size()), SPC.begin() + GAME);
			std::copy_n(musics[i].comment.cbegin(), std::min(32u, musics[i].comment.size()), SPC.begin() + COMMENT);
			std::copy_n(musics[i].author.cbegin(), std::min(32u, musics[i].author.size()), SPC.begin() + AUTHOR);
		}

		std::copy_n(programData.cbegin(), programSize, SPC.begin() + RAM + programPos);

		int backupIndex = i;
		if (mode == MUSIC)		// // //
			std::copy(musics[i].finalData.cbegin(), musics[i].finalData.cend(), SPC.begin() + RAM + localPos);		// // //
		else
			i = highestGlobalSong + 1;		// While dumping SFX, pretend that the current song is the lowest local song

		int tablePos = localPos + musics[i].finalData.size();
		if ((tablePos & 0xFF) != 0)
			tablePos = (tablePos & 0xFF00) + 0x100;

		int samplePos = tablePos + musics[i].mySamples.size() * 4;
		auto srcTable = SPC.begin() + RAM + tablePos;

		for (const auto &id : musics[i].mySamples) {		// // //
			const auto &samp = samples[id];
			unsigned short loopPoint = samp.loopPoint;
			unsigned short newLoopPoint = loopPoint + samplePos;

			*srcTable++ = static_cast<uint8_t>(samplePos & 0xFF);
			*srcTable++ = static_cast<uint8_t>(samplePos >> 8);
			*srcTable++ = static_cast<uint8_t>(newLoopPoint & 0xFF);
			*srcTable++ = static_cast<uint8_t>(newLoopPoint >> 8);

			std::copy(samp.data.cbegin(), samp.data.cend(), SPC.begin() + RAM + samplePos);
			samplePos += samp.data.size();
		}

		std::copy(DSPBase.cbegin(), DSPBase.cend(), SPC.begin() + DSP_ADDR);		// // //

		SPC[DSP_ADDR + 0x5D] = tablePos >> 8; // sample directory

		SPC[RAM + 0x5F] = 0x20;

		SPC[LENGTH    ] = (musics[i].seconds / 100 % 10) + '0';		// Why on Earth is the value stored as plain text...?
		SPC[LENGTH + 1] = (musics[i].seconds / 10 % 10) + '0';
		SPC[LENGTH + 2] = (musics[i].seconds / 1 % 10) + '0';

		SPC[FADEOUT    ] = '1';
		SPC[FADEOUT + 1] = '0';
		SPC[FADEOUT + 2] = '0';
		SPC[FADEOUT + 3] = '0';
		SPC[FADEOUT + 4] = '0';

		SPC[PC    ] = static_cast<uint8_t>(programUploadPos & 0xFF);	// // // Set the PC to the main loop.
		SPC[PC + 1] = static_cast<uint8_t>(programUploadPos >> 8);	// The values of the registers (besides stack which is in the file) don't matter.  They're 0 in the base file.

		i = backupIndex;

		switch (mode) {		// // //
		case MUSIC: SPC[RAM + 0xF6] = highestGlobalSong + 1; break;	// Tell the SPC to play this song.
		case SFX1:  SPC[RAM + 0xF4] = i; break;						// Tell the SPC to play this SFX
		case SFX2:  SPC[RAM + 0xF7] = i; break;						// Tell the SPC to play this SFX
		}
		if (yoshi)		// // //
			SPC[RAM + 0xF5] = 2;

		std::stringstream timeField;		// // //
		std::time_t t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
		timeField << std::put_time(std::localtime(&t), "%m/%d/%Y");
		auto timeStr = timeField.str();
		std::copy_n(timeStr.cbegin(), std::min(10u, timeStr.size()), SPC.begin() + DATE);

		auto fname = fs::path {mode == SFX1 ? soundEffects[0][i].name :
			mode == SFX2 ? soundEffects[1][i].name : musics[i].getFileName()}.stem();		// // //
		switch (mode) {
		case SFX1: fname = "1DF9" / fname; break;
		case SFX2: fname = "1DFC" / fname; break;
		}

		fname = "SPCs" / fname;
		if (yoshi)		// // //
			fname.replace_filename(fname.filename().string() + " (Yoshi)");
		fname.replace_extension(".spc");

		if (verbose)
			std::cout << "Wrote \"" << fname << "\" to file." << std::endl;

		writeFile(fname, SPC);
		++SPCsGenerated;		// // //
	};

//	std::time_t recentMod = getLastModifiedTime();		// // // If any main program modifications were made, we need to update all SPCs.

	for (spc_mode_t mode : {MUSIC, SFX1, SFX2}) {
		if (!sfxDump && mode != MUSIC)		// // //
			break;

		for (int i = 0; i < 256; i++) {		// // //
			switch (mode) {
			case MUSIC:
				// Cannot generate SPCs for global songs as required samples, SRCN table, etc. cannot be determined.
				if (!musics[i].exists || i <= highestGlobalSong)
					continue;
				break;
			case SFX1:
				if (!soundEffects[0][i].exists)
					continue;
				break;
			case SFX2:
				if (!soundEffects[1][i].exists)
					continue;
				break;
			}
			//time_t spcTimeStamp = getTimeStamp((File)fname);

			/*if (!forceSPCGeneration)
				if (fileExists(fname))
				if (getTimeStamp((File)("music/" + musics[i].name)) < spcTimeStamp)
				if (getTimeStamp((File)"./samples") < spcTimeStamp)
				if (recentMod < spcTimeStamp)
				continue;*/

			if (mode == MUSIC && musics[i].hasYoshiDrums)
				makeSPCfn(i, mode, true);
			makeSPCfn(i, mode, false);
		}
	}

	if (verbose) {
		if (SPCsGenerated == 1)
			std::cout << "Generated 1 SPC file." << std::endl;
		else if (SPCsGenerated > 0)
			std::cout << "Generated " << SPCsGenerated << " SPC files." << std::endl;
	}
}

void assembleSNESDriver2() {
	if (verbose)
		std::cout << "\nGenerating SNES driver...\n" << std::endl;

	std::string patch = openTextFile("asm/SNES/patch.asm");		// // //

	//removeFile("asm/SNES/temppatch.sfc");

	//writeTextFile("asm/SNES/temppatch.asm", patch);

	//execute("asar asm/SNES/temppatch.asm 2> temp.log");
	//if (fileExists("temp.log"))
	//{
	//	std::cout << "asar reported an error assembling patch.asm. Refer to temp.log for details." << std::endl;
	//	quit(1);
	//}

	//std::vector<uint8_t> patchBin;		// // //
	//openFile("asm/SNES/temppatch.sfc", patchBin);

	insertValue(reuploadPos, 4, "!ExpARAMRet = ", patch);
	insertValue(SRCNTableCodePos, 4, "!TabARAMRet = ", patch);
	insertValue(mainLoopPos, 4, "!DefARAMRet = ", patch);
	insertValue(songCount, 2, "!SongCount = ", patch);

	int pos = patch.find("MusicPtrs:");		// // //
	if (pos == -1)
		fatalError("Error: \"MusicPtrs:"" could not be found.");

	patch = patch.substr(0, pos) + openTextFile("asm/SNES/patch2.asm");		// // //

	std::stringstream musicPtrStr; musicPtrStr << "MusicPtrs: \ndl ";
	std::stringstream samplePtrStr; samplePtrStr << "\n\nSamplePtrs:\ndl ";
	std::stringstream sampleLoopPtrStr; sampleLoopPtrStr << "\n\nSampleLoopPtrs:\ndw ";
	std::stringstream musicIncbins; musicIncbins << "\n\n";
	std::stringstream sampleIncbins; sampleIncbins << "\n\n";

	if (verbose)
		std::cout << "Writing music files..." << std::endl;

	for (int i = 0; i < songCount; i++) {
		if (musics[i].exists == true && i > highestGlobalSong) {
			std::stringstream musicBinPath;
			musicBinPath << "asm/SNES/bin/music" << hex2 << i << ".bin";
			unsigned requestSize = static_cast<unsigned>(fs::file_size(musicBinPath.str()));		// // //
			int freeSpace = findFreeSpace(requestSize, bankStart, rom);
			if (freeSpace == -1)
				fatalError("Error: Your ROM is out of free space.");

			freeSpace = PCToSNES(freeSpace);
			musicPtrStr << "music" << hex2 << i << "+8";
			musicIncbins << "org $" << hex6 << freeSpace << "\nmusic" << hex2 << i << ": incbin \"bin/music" << hex2 << i << ".bin\"" << std::endl;
		}
		else {
			musicPtrStr << "$" << hex6 << 0;
		}

		if ((i & 0xF) == 0xF && i != songCount - 1)
			musicPtrStr << "\ndl ";
		else if (i != songCount - 1)
			musicPtrStr << ", ";
	}

	if (verbose)
		std::cout << "Writing sample files..." << std::endl;

	for (size_t i = 0, n = samples.size(); i < n; ++i) {		// // //
		if (samples[i].exists) {
			const size_t ssize = samples[i].data.size();		// // //
			std::vector<uint8_t> temp {
				'S', 'T', 'A', 'R',
				static_cast<uint8_t>((ssize + 1) & 0xFF), static_cast<uint8_t>((ssize + 1) >> 8),
				static_cast<uint8_t>(~(ssize + 1) & 0xFF), static_cast<uint8_t>(~(ssize + 1) >> 8),
				static_cast<uint8_t>(ssize & 0xFF), static_cast<uint8_t>(ssize >> 8),
			};
			temp.insert(temp.cend(), samples[i].data.cbegin(), samples[i].data.cend());		// // //

			std::stringstream filename;
			filename << "asm/SNES/bin/brr" << hex2 << i << ".bin";
			writeFile(filename.str(), temp);

			unsigned requestSize = static_cast<unsigned>(fs::file_size(filename.str()));		// // //
			int freeSpace = findFreeSpace(requestSize, bankStart, rom);
			if (freeSpace == -1)
				fatalError("Error: Your ROM is out of free space.");

			freeSpace = PCToSNES(freeSpace);
			samplePtrStr << "brr" << hex2 << i << "+8";
			sampleIncbins << "org $" << hex6 << freeSpace << "\nbrr" << hex2 << i << ": incbin \"bin/brr" << hex2 << i << ".bin\"" << std::endl;

		}
		else
			samplePtrStr << "$" << hex6 << 0;

		sampleLoopPtrStr << "$" << hex4 << samples[i].loopPoint;

		if ((i & 0xF) == 0xF && i != samples.size() - 1) {
			samplePtrStr << "\ndl ";
			sampleLoopPtrStr << "\ndw ";
		}
		else if (i != samples.size() - 1) {
			samplePtrStr << ", ";
			sampleLoopPtrStr << ", ";
		}
	}

	patch += "pullpc\n\n";

	musicPtrStr << "\ndl $FFFFFF\n";
	samplePtrStr << "\ndl $FFFFFF\n";

	patch += musicPtrStr.str();
	patch += samplePtrStr.str();
	patch += sampleLoopPtrStr.str();

	//patch += "";

	patch += musicIncbins.str();
	patch += sampleIncbins.str();

	insertValue(highestGlobalSong, 2, "!GlobalMusicCount = #", patch);

	std::stringstream ss;
	ss << "\n\norg !SPCProgramLocation" << "\nincbin \"bin/main.bin\"";
	patch += ss.str();

	remove("asm/SNES/temppatch.sfc");

	std::string undoPatch = openTextFile("asm/SNES/AMUndo.asm");		// // //
	patch.insert(patch.cbegin(), undoPatch.cbegin(), undoPatch.cend());

	writeTextFile("asm/SNES/temppatch.asm", patch);

	if (verbose)
		std::cout << "Final compilation..." << std::endl;

	if (!doNotPatch) {

		//execute("asar asm/SNES/temppatch.asm asm/SNES/temp.sfc 2> temp.log");
		//if (fileExists("temp.log"))
		if (!asarPatchToROM("asm/SNES/temppatch.asm", "asm/SNES/temp.sfc"))
			fatalError("asar reported an error.  Refer to temp.log for details.");		// // //

		//execute("asar asm/SNES/tweaks.asm asm/SNES/temp.sfc 2> temp.log");
		//if (fileExists("temp.log"))
		//	fatalError("asar reported an error.  Refer to temp.log for details.");

		//execute("asar asm/SNES/NMIFix.asm asm/SNES/temp.sfc 2> temp.log");
		//if (fileExists("temp.log"))
		//	fatalError("asar reported an error.  Refer to temp.log for details.");

		std::vector<uint8_t> final;		// // //
		final = romHeader;

		std::vector<uint8_t> tempsfc = openFile("asm/SNES/temp.sfc");		// // //
		final.insert(final.cend(), tempsfc.cbegin(), tempsfc.cend());		// // //

		// // //
		fs::remove(ROMName.string() + "~");
		fs::rename(ROMName, ROMName.string() + "~");

		writeFile(ROMName, final);
	}
}

void generateMSC() {
	fs::path mscname = ROMName.replace_extension(".msc");		// // //

	std::stringstream text;

	for (int i = 0; i < 256; i++) {
		if (musics[i].exists) {
			text << hex2 << i << "\t" << 0 << "\t" << musics[i].title << "\n";
			text << hex2 << i << "\t" << 1 << "\t" << musics[i].title << "\n";
		}
	}
	writeTextFile(mscname, text.str());
}

void cleanUpTempFiles() {
	if (doNotPatch)		// If this is specified, then the user might need these temp files.  Keep them.
		return;

	removeFile("asm/tempmain.asm");
	removeFile("asm/main.bin");
	removeFile("asm/SFX1DF9Table.bin");
	removeFile("asm/SFX1DFCTable.bin");
	removeFile("asm/SFXData.bin");

	removeFile("asm/SNES/temppatch.asm");
	removeFile("asm/SNES/temp.sfc");
	removeFile("temp.log");
	removeFile("temp.txt");
}

void tryToCleanSampleToolData() {
	const char HEADER_STR[] = "New Super Mario World Sample Utility 2.0 by smkdan";		// // //
	auto it = std::search(rom.cbegin(), rom.cend(), std::cbegin(HEADER_STR), std::cend(HEADER_STR));
	if (it == rom.cend())
		return;

	std::cout << "Sample Tool detected.  Erasing data..." << std::endl;

	unsigned int i = std::distance(rom.cbegin(), it);
	int hackPos = i - 8;

	i += 0x36;

	int sizeOfErasedData = 0;

	bool removed[0x100] = { };		// // //
	for (int j = 0; j < 0x207; j++) {
		if (removed[rom[j + i]]) continue;
		sizeOfErasedData += clearRATS(SNESToPC(rom[j + i] * 0x10000 + 0x8000));
		removed[rom[j + i]] = true;
	}

	int sampleDataSize = sizeOfErasedData;

	sizeOfErasedData += clearRATS(hackPos);

	std::cout << "Erased 0x" << hex6 << sizeOfErasedData << " bytes, of which 0x" << sampleDataSize << " were sample data.";
}

void tryToCleanAM4Data() {
	if ((rom.size() % 0x8000 != 0 && rom[0x1940] == 0x22) || (rom.size() % 0x8000 == 0 && rom[0x1740] == 0x22)) {
		if (rom.size() % 0x8000 == 0)
			fatalError("Addmusic 4.05 ROMs can only be cleaned if they have a header. This does not\n"
					   "apply to any other aspect of the program.");		// // //

		std::cout << "Attempting to erase data from Addmusic 4.05:" << std::endl;
		std::string ROMstr = ROMName.string();		// // //
		char **am405argv = new char*[2];
		am405argv[1] = const_cast<char *>(ROMstr.c_str());
		removeAM405Data(2, am405argv);
		delete[] am405argv;

		rom = openFile(ROMName);		// // // Reopen the file.
		if (rom[0x255] == 0x5C) {
			int moreASMData = ((rom[0x255 + 3] << 16) | (rom[0x255 + 2] << 8) | (rom[0x255 + 1])) - 8;
			clearRATS(SNESToPC(moreASMData));
		}
		int romiSPCProgramAddress = (unsigned char)rom[0x2E9] | ((unsigned char)rom[0x2EE] << 8) | ((unsigned char)rom[0x2F3] << 16);
		clearRATS(SNESToPC(romiSPCProgramAddress) - 12 + 0x200);
	}
}

void tryToCleanAMMData() {
	if ((rom.size() % 0x8000 != 0 && rom[0x078200] == 0x53) || (rom.size() % 0x8000 == 0 && rom[0x078000] == 0x53)) {		// Since RATS tags only need to be in banks 0x10+, a tag here signals AMM usage.
		if (rom.size() % 0x8000 == 0)
			fatalError("AddmusicM ROMs can only be cleaned if they have a header. This does not\n"
					   "apply to any other aspect of the program.");		// // //

		if (!fs::exists("INIT.asm"))		// // //
			fatalError("AddmusicM was detected.  In order to remove it from this ROM, you must put\n"
					   "AddmusicM's INIT.asm as well as xkasAnti and a clean ROM (named clean.smc) in\n"
					   "the same folder as this program. Then attempt to run this program once more.");

		std::cout << "AddmusicM detected.  Attempting to remove..." << std::endl;
		execute("perl addmusicMRemover.pl " + ROMName.string());		// // //
		execute("xkasAnti clean.smc " + ROMName.string() + " INIT.asm");
	}
}

void checkMainTimeStamps()			// Disabled for now, as this only works if the ROM is linked to the program (so it wouldn't work if the program was used on multiple ROMs)
{						// It didn't save much time anyway...
	recompileMain = true;
	return;

/*
	if (!fs::exists("asm/SNES/bin/main.bin") || 0 != ::strncmp((char *)(rom.data() + 0x70000), "@AMK", 4)) {		// // //
		recompileMain = true;
		return;
	}

	mostRecentMainModification = std::max(mostRecentMainModification, getLastModifiedTime());		// // //
	mostRecentMainModification = std::max(mostRecentMainModification, getTimeStamp("asm/SNES/patch.asm"));
	mostRecentMainModification = std::max(mostRecentMainModification, getTimeStamp("asm/SNES/patch2.asm"));
	mostRecentMainModification = std::max(mostRecentMainModification, getTimeStamp("asm/SNES/tweaks.asm"));
	mostRecentMainModification = std::max(mostRecentMainModification, getTimeStamp("Addmusic_list.txt"));

	if (recompileMain = (mostRecentMainModification > getTimeStamp("asm/SNES/bin/main.bin")))
		std::cout << "Changes have been made to the global program.  Recompiling...\n" << std::endl;
*/
}

void generatePNGs()
{
	for (auto &current : musics)
	{
		if (current.index <= highestGlobalSong) continue;
		if (current.exists == false) continue;

		std::vector<unsigned char> bitmap;
		// 1024 pixels wide, 64 pixels tall, 4 bytes per pixel

		const int width = 1024;
		const int height = 64;

		bitmap.resize(width * height * 4);

		int x = 0;
		int y = 0;


		for (int i = 0; i < width * height; i++)
		{
			unsigned char r = 0;
			unsigned char g = 0;
			unsigned char b = 0;
			unsigned char a = 255;

			if (i >= 0 && i < programUploadPos)
			{
				r = 255;
			}
			else if (i >= programUploadPos && i < programPos + programSize)
			{
				r = 255;
				g = 255;
			}
			else if (i >= current.spaceInfo.songStartPos && i < current.spaceInfo.songEndPos)
			{
				g = 128;
			}
			else if (i >= current.spaceInfo.sampleTableStartPos && i < current.spaceInfo.sampleTableEndPos)
			{
				g = 255;
			}
			else if (i >= current.spaceInfo.individualSampleStartPositions[0] && i < current.spaceInfo.individualSampleEndPositions[current.spaceInfo.individualSampleEndPositions.size() - 1])
			{
				int currentSampleIndex = 0;

				for (auto currentSampleEndPos : current.spaceInfo.individualSampleEndPositions)
				{
					if (currentSampleEndPos > i) break;

					currentSampleIndex++;
				}

				bool sampleIsImportant = current.spaceInfo.individialSampleIsImportant[currentSampleIndex];

				int sampleCount = current.spaceInfo.individualSampleStartPositions.size();

				b = static_cast<unsigned char>(static_cast<double>(currentSampleIndex) / static_cast<double>(sampleCount)* 127.0 + 128.0);


				if (sampleIsImportant)
				{
					g = static_cast<unsigned char>(static_cast<double>(currentSampleIndex) / static_cast<double>(sampleCount)* 127.0 + 128.0);
				}
			}
			else if (i >= current.spaceInfo.echoBufferStartPos && i < current.spaceInfo.echoBufferEndPos)
			{
				r = 160;
				b = 160;
			}
			else if (i >= current.spaceInfo.echoBufferEndPos)
			{
				r = 63;
				b = 63;
				g = 63;
			}

			int bitmapIndex = y * width + x;


			if ((bitmapIndex*4)+3 >= bitmap.size())
				break;				// This should never happen, but let's be safe.

			bitmap[(bitmapIndex*4)+0] = r;
			bitmap[(bitmapIndex*4)+1] = g;
			bitmap[(bitmapIndex*4)+2] = b;
			bitmap[(bitmapIndex*4)+3] = a;

			y++;
			if (y >= height)
			{
				y = 0;
				x++;
			}
		}

		auto path = current.pathlessSongName;
		path = "Visualizations/" + path + ".png";
		lodepng::encode(path, bitmap, width, height);


	}


}


