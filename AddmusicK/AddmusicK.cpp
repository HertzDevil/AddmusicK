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
#include "Music.h"		// // //
#include "MML/Lexer.h"		// // //

bool waitAtEnd = true;
fs::path ROMName;		// // //

Music musics[256];		// // //
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
//std::time_t getLastModifiedTime();		// // //

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
	auto startTime = std::chrono::steady_clock::now();		// // //

	std::cout << "AddmusicK version " << AMKVERSION << "." << AMKMINOR << "." << AMKREVISION << " by Kipernal\n";
	std::cout << "Parser version " << PARSER_VERSION << "\n\n";
	std::cout << "Protip: Be sure to read the readme! If there's an error or something doesn't\nseem right, it may have your answer!\n\n\n";

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

	auto elapsed = std::chrono::duration<double> {std::chrono::steady_clock::now() - startTime}.count();		// // //

	std::cout << "\nSuccess!\n\n";
	if (elapsed - 1 < 0.02)
		std::cout << "Completed in 1 second.\n";
	else
		std::cout << "Completed in " << std::dec << std::setprecision(2) << std::fixed << elapsed << " seconds.\n";

	if (waitAtEnd)
		quit(0);
	return 0;
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
				 "your ROM, or your ROM is corrupted. Continue? (Y or N)\n";
			std::cout << s.str();

			int c = '\0';
			while (c != 'y' && c != 'Y' && c != 'n' && c != 'N')
				c = getchar();

			if (c == 'n' || c == 'N')
				quit(1);
		}

		int romversion = *(unsigned char *)(rom.data() + SNESToPC(0x0E8004));

		if (romversion > DATA_VERSION) {
			std::cout << "WARNING: This ROM was modified using a newer version of AddmusicK.\n";
			std::cout << "You can continue, but it is HIGHLY recommended that you upgrade AMK first.\n";
			std::cout << "Continue anyway? (Y or N)\n";

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

// // // moved
int scanInt(const std::string &str, const std::string &value) {		// Scans an integer value that comes after the specified string within another string.  Must be in $XXXX format (or $XXXXXX, etc.).
	int i = str.find(value);		// // //
	if (i == std::string::npos)
		fatalError("Error: Could not find \"" + value + "\"");		// // //

	int ret;
	std::sscanf(str.c_str() + i + value.length(), "$%X", &ret);	// Woo C functions in C++ code!
	return ret;
}

void assembleSNESDriver() {
	programUploadPos = scanInt(openTextFile("asm/SNES/patch.asm"), "!DefARAMRet = ");		// // //
}

void assembleSPCDriver() {
	removeFile("temp.log");		// // //
	removeFile("asm/main.bin");

	programPos = scanInt(openTextFile("asm/main.asm"), "base ");		// // //
	if (verbose)
		std::cout << "Compiling main SPC program, pass 1.\n";

	//execute("asar asm/main.asm asm/main.bin 2> temp.log > temp.txt");

	//if (fileExists("temp.log"))
	if (!asarCompileToBIN("asm/main.asm", "asm/main.bin"))
		fatalError("asar reported an error while assembling asm/main.asm. Refer to temp.log for\ndetails.");		// // //

	std::string temptxt = openTextFile("temp.txt");		// // //
	mainLoopPos = scanInt(temptxt, "MainLoopPos: ");
	reuploadPos = scanInt(temptxt, "ReuploadPos: ");
	SRCNTableCodePos = scanInt(temptxt, "SRCNTableCodePos: ");

	removeFile("temp.log");

	programSize = static_cast<unsigned>(fs::file_size("asm/main.bin"));		// // //
}

void loadMusicList() {
	const std::string SONG_LIST {"Addmusic_list.txt"};
	AMKd::MML::SourceFile list {openTextFile(SONG_LIST)};		// // //
	using namespace AMKd::MML::Lexer;

	bool inGlobals = false;
	bool inLocals = false;
	int shallowSongCount = 0;

	highestGlobalSong = -1;		// // //

	while (list.HasNextToken()) {		// // //
		if (list.Trim("Globals:")) {
			inGlobals = true;
			inLocals = false;
		}
		else if (list.Trim("Locals:")) {
			inGlobals = false;
			inLocals = true;
		}
		else {
			if (!inGlobals && !inLocals)
				fatalError("Error: Could not find \"Globals:\" or \"Locals:\" label.",
						   SONG_LIST, list.GetLineNumber());		// // //

			auto param = GetParameters<HexInt>(list);
			if (!param || param.get<0>() > 0xFFu || !list.SkipSpaces())
				fatalError("Invalid song index.", SONG_LIST, list.GetLineNumber());
			int index = param.get<0>();
			if (inGlobals)
				highestGlobalSong = std::max(highestGlobalSong, index);
			else if (inLocals && index <= highestGlobalSong)
				fatalError("Error: Local song numbers must be lower than the largest global song number.",
						   SONG_LIST, list.GetLineNumber());		// // //

			auto name = list.Trim("[^\\r\\n]+");
			if (!name)
				fatalError("Error: Could not read file name.", SONG_LIST, list.GetLineNumber());
			musics[index].name = *name;
			musics[index].exists = true;
//			if (inLocals && !justSPCsPlease)
//				musics[index].text = openTextFile(fs::path("music") / tempName);		// // //
			++shallowSongCount;
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
	const std::string SAMPGROUP_NAME {"Addmusic_sample groups.txt"};
	AMKd::MML::SourceFile list {openTextFile(SAMPGROUP_NAME)};		// // //
	using namespace AMKd::MML::Lexer;

	while (list.HasNextToken()) {		// // //
		auto nameParam = GetParameters<Sep<'#'>, String, Sep<'{'>>(list);
		if (!nameParam)
			fatalError("Error: Could not find sample group name.", SAMPGROUP_NAME, list.GetLineNumber());
		BankDefine sg;		// // //
		sg.name = nameParam.get<0>();

		while (auto item = GetParameters<Sep<'\"'>, QString, Opt<Sep<'!'>>>(list)) {
			sg.samples.push_back(item.get<0>());
			sg.importants.push_back(item.get<1>().has_value());
		}

		if (!GetParameters<Sep<'}'>>(list))
			fatalError("Error: Unexpected character in sample group definition.", SAMPGROUP_NAME, list.GetLineNumber());
		bankDefines.push_back(std::move(sg));
	}

/*
	for (const auto &def : bankDefines) {
		for (const auto &samp : def.samples) {
			if (!std::any_of(samples.cbegin(), samples.cend(), [&] (const Sample &s) { return s.name == samp; })) {
				//loadSample(samp, &samples[samples.size()]);
				samples[samples.size()].exists = true;
			}
		}
	}
*/
}

void loadSFXList() {		// Very similar to loadMusicList, but with a few differences.
	const std::string SFX_LIST {"Addmusic_sound effects.txt"};
	AMKd::MML::SourceFile list {openTextFile(SFX_LIST)};		// // //
	using namespace AMKd::MML::Lexer;

	int bank = -1;		// // //
	int SFXCount = 0;
	const fs::path BANK_FOLDER[SFX_BANKS] = {"1DF9", "1DFC"};

	while (list.HasNextToken()) {		// // //
		if (list.Trim("SFX1DF9:"))
			bank = 0;
		else if (list.Trim("SFX1DFC:"))
			bank = 1;
		else {
			if (bank == -1)
				fatalError("Error: Could not find \"SFX1DF9:\" or \"SFX1DFC:\" label.",
						   SFX_LIST, list.GetLineNumber());		// // //

			auto param = GetParameters<HexInt>(list);
			if (!param || param.get<0>() > 0xFFu || !list.SkipSpaces())
				fatalError("Invalid sound effect index.", SFX_LIST, list.GetLineNumber());
			int index = param.get<0>();

			bool isPointer = list.Trim('*').has_value();
			bool add0 = !isPointer && !list.Trim('?');
			auto name = (list.SkipSpaces(), list.Trim("[^\\r\\n]+"));
			if (!name)
				fatalError("Error: Could not read file name.", SFX_LIST, list.GetLineNumber());

			auto &samp = soundEffects[bank][index];
			(isPointer ? samp.pointName : samp.name) = *name;		// // //
			samp.exists = true;
			samp.add0 = add0;
			if (!isPointer)
				samp.text = openTextFile(BANK_FOLDER[bank] / *name);
			++SFXCount;
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
	int dataTotal[SFX_BANKS] = { };		// // //
	int sfxCount[SFX_BANKS] = { };
	std::vector<uint16_t> sfxPointers[SFX_BANKS];

	const auto getSFXSpace = [&] (int bank) {		// // //
		return sfxCount[bank] * 2 + dataTotal[bank];
	};

	for (int bank = 0; bank < SFX_BANKS; ++bank)
		for (int i = 255; i >= 0; i--) {
			if (soundEffects[bank][i].exists) {
				sfxCount[bank] = i;
				break;
			}
		}

	std::vector<uint8_t> allSFXData;		// // //

	for (int bank = 0; bank < SFX_BANKS; ++bank) {		// // //
		for (int i = 0; i <= sfxCount[bank]; ++i) {
			auto &sfx = soundEffects[bank][i];		// // //
			if (sfx.exists && !sfx.pointsTo) {
				sfx.posInARAM = getSFXSpace(0) + getSFXSpace(1) + programSize + programPos;
				sfx.compile();
				sfxPointers[bank].push_back(sfx.posInARAM);
				dataTotal[bank] += sfx.data.size() + sfx.code.size();
			}
			else if (!sfx.exists)
				sfxPointers[bank].push_back(0xFFFF);
			else if (i > sfx.pointsTo)
				sfxPointers[bank].push_back(sfxPointers[bank][sfx.pointsTo]);
			else
				fatalError("Error: A sound effect that is a pointer to another sound effect must come after\n"
						   "the sound effect that it points to.");
			allSFXData.insert(allSFXData.cend(), sfx.data.cbegin(), sfx.data.cend());		// // //
			allSFXData.insert(allSFXData.cend(), sfx.code.cbegin(), sfx.code.cend());
		}
	}

	if (errorCount > 0)
		fatalError("There were errors when compiling the sound effects.  Compilation aborted.  Your\n"
				   "ROM has not been modified.");

	if (verbose) {
		std::cout << "Total space used by 1DF9 sound effects: 0x" << hex4 << getSFXSpace(0) << '\n';
		std::cout << "Total space used by 1DFC sound effects: 0x" << hex4 << getSFXSpace(1) << '\n';
	}

	std::cout << "Total space used by all sound effects: 0x" << hex4 << (getSFXSpace(0) + getSFXSpace(1)) << std::dec << '\n';

	for (auto &x : sfxPointers)		// // //
		x.erase(x.begin(), x.begin() + 1);

	writeFile("asm/SFX1DF9Table.bin", sfxPointers[0]);
	writeFile("asm/SFX1DFCTable.bin", sfxPointers[1]);
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
		std::cout << "Compiling main SPC program, pass 2.\n";

	//execute("asar asm/tempmain.asm asm/main.bin 2> temp.log > temp.txt");
	//if (fileExists("temp.log")) 
	if (!asarCompileToBIN("asm/tempmain.asm", "asm/main.bin"))
		fatalError("asar reported an error while assembling asm/main.asm. Refer to temp.log for\ndetails.\n");		// // //

	programSize = static_cast<unsigned>(fs::file_size("asm/main.bin"));		// // //

	std::cout << "Total size of main program + all sound effects: 0x" << hex4 << programSize << std::dec << '\n';
}

void compileMusic() {
	if (verbose)
		std::cout << "Compiling music...\n";

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

	tempstream << "org $" << hex6 << PCToSNES(findFreeSpace(songSampleListSize, bankStart, rom)) << "\n\n\n";

	s.insert(0, tempstream.str());

	writeTextFile("asm/SNES/SongSampleList.asm", s);
}

void fixMusicPointers() {
	if (verbose)
		std::cout << "Fixing song pointers...\n";

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
			std::cout << "Compiling main SPC program, final pass.\n";

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
		std::cout << "Total space in ARAM left for local songs: 0x" << hex4 << (0x10000 - programSize - 0x400) << " bytes." << std::dec << '\n';

	int defaultIndex = -1, optimizedIndex = -1;
	for (unsigned int i = 0; i < bankDefines.size(); i++) {
		if (bankDefines[i].name == "default")
			defaultIndex = i;
		if (bankDefines[i].name == "optimized")
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

		std::cout << "Total space in ARAM for local songs using #default: 0x" << hex4 << (0x10000 - programSize - 0x400 - groupSize) << " bytes." << std::dec << '\n';
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

		std::cout << "Total space in ARAM for local songs using #optimized: 0x" << hex4 << (0x10000 - programSize - 0x400 - groupSize) << " bytes." << std::dec << '\n';
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
#pragma warning ( suppress : 4996 )
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
			std::cout << "Wrote \"" << fname << "\" to file.\n";

		writeFile(fname, SPC);
		++SPCsGenerated;		// // //
	};

//	std::time_t recentMod = getLastModifiedTime();		// // // If any main program modifications were made, we need to update all SPCs.

	// Cannot generate SPCs for global songs as required samples, SRCN table, etc. cannot be determined.
	for (int i = highestGlobalSong + 1; i < 256; ++i)		// // //
		if (musics[i].exists) {
			//time_t spcTimeStamp = getTimeStamp((File)fname);

			/*if (!forceSPCGeneration)
				if (fileExists(fname))
				if (getTimeStamp((File)("music/" + musics[i].name)) < spcTimeStamp)
				if (getTimeStamp((File)"./samples") < spcTimeStamp)
				if (recentMod < spcTimeStamp)
				continue;*/
			if (musics[i].hasYoshiDrums)
				makeSPCfn(i, MUSIC, true);
			makeSPCfn(i, MUSIC, false);
		}

	if (sfxDump)
		for (int i = 0; i < 256; i++) {		// // //
			if (soundEffects[0][i].exists)
				makeSPCfn(i, SFX1, false);
			if (soundEffects[1][i].exists)
				makeSPCfn(i, SFX2, false);
		}

	if (verbose) {
		if (SPCsGenerated == 1)
			std::cout << "Generated 1 SPC file.\n";
		else if (SPCsGenerated > 0)
			std::cout << "Generated " << SPCsGenerated << " SPC files.\n";
	}
}

void assembleSNESDriver2() {
	if (verbose)
		std::cout << "\nGenerating SNES driver...\n\n";

	std::string patch = openTextFile("asm/SNES/patch.asm");		// // //

	//removeFile("asm/SNES/temppatch.sfc");

	//writeTextFile("asm/SNES/temppatch.asm", patch);

	//execute("asar asm/SNES/temppatch.asm 2> temp.log");
	//if (fileExists("temp.log"))
	//{
	//	std::cout << "asar reported an error assembling patch.asm. Refer to temp.log for details.\n";
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
		std::cout << "Writing music files...\n";

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
			musicIncbins << "org $" << hex6 << freeSpace << "\nmusic" << hex2 << i << ": incbin \"bin/music" << hex2 << i << ".bin\"\n";
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
		std::cout << "Writing sample files...\n";

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
			sampleIncbins << "org $" << hex6 << freeSpace << "\nbrr" << hex2 << i << ": incbin \"bin/brr" << hex2 << i << ".bin\"\n";

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
		std::cout << "Final compilation...\n";

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

	std::cout << "Sample Tool detected.  Erasing data...\n";

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

		std::cout << "Attempting to erase data from Addmusic 4.05:\n";
		std::string ROMstr = ROMName.string();		// // //
		char *am405argv[] = {"", const_cast<char *>(ROMstr.c_str())};
		removeAM405Data(2, am405argv);

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

		std::cout << "AddmusicM detected.  Attempting to remove...\n";
		execute("perl addmusicMRemover.pl " + ROMName.string(), false);		// // //
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
		std::cout << "Changes have been made to the global program.  Recompiling...\n\n";
*/
}

// // //
/*
std::time_t getLastModifiedTime() {
	std::time_t recentMod = 0;			// If any main program modifications were made, we need to update all SPCs.
	for (int i = 1; i <= highestGlobalSong; i++)
		recentMod = std::max(recentMod, getTimeStamp(fs::path("music") / musics[i].getFileName()));		// // //

	recentMod = std::max(recentMod, getTimeStamp("asm/main.asm"));
	recentMod = std::max(recentMod, getTimeStamp("asm/commands.asm"));
	recentMod = std::max(recentMod, getTimeStamp("asm/InstrumentData.asm"));
	recentMod = std::max(recentMod, getTimeStamp("asm/CommandTable.asm"));
	recentMod = std::max(recentMod, getTimeStamp("Addmusic_sound effects.txt"));
	recentMod = std::max(recentMod, getTimeStamp("Addmusic_sample groups.txt"));
	recentMod = std::max(recentMod, getTimeStamp("AddmusicK.exe"));

	for (int i = 1; i < 256; i++) {		// // //
		if (soundEffects[0][i].exists)
			recentMod = std::max(recentMod, getTimeStamp(fs::path("1DF9") / soundEffects[0][i].getEffectiveName()));
		if (soundEffects[1][i].exists)
			recentMod = std::max(recentMod, getTimeStamp(fs::path("1DFC") / soundEffects[1][i].getEffectiveName()));
	}

	return recentMod;
}
*/

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


