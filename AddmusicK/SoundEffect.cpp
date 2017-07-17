#include "SoundEffect.h"
#include "globals.h"		// // //
#include "MML/Preprocessor.h"		// // //
#include "Binary/Defines.h"		// // //
#include <sstream>
#include <algorithm>		// // //
#include "Utility/Swallow.h"		// // //
#include "Utility/StaticTrie.h"		// // //
#include "Utility/Exception.h"		// // //
#include "MML/Lexer.h"		// // //

// // //
template <typename T, typename U>
T requires(T x, T min, T max, U&& err) {
	return (x >= min && x <= max) ? x :
		throw AMKd::Utility::ParamException {std::forward<U>(err)};
}

// // //
template <typename... Args>
void SoundEffect::append(Args&&... value) {
#if 0
	(dataStream << static_cast<uint8_t>(value), ...);
#else
	SWALLOW(dataStream << static_cast<uint8_t>(value));
#endif
}

const std::string &SoundEffect::getEffectiveName() const {
	return name == "" ? pointName : name;		// // //
}

int SoundEffect::getPitch(int letter, int octave) {
	static const int pitches[] = {9, 11, 0, 2, 4, 5, 7};

	letter = pitches[letter - 0x61] + (octave - 1) * 12 + 0x80;

	if (mml_.Trim('+'))
		++letter;
	else if (mml_.Trim('-'))
		--letter;

	if (letter < 0x80)
		return AMKd::Binary::CmdType::Rest; // -1;
	if (letter >= 0xC6)
		return AMKd::Binary::CmdType::Rest; // -2;

	return letter;
}

int SoundEffect::getNoteLength() {
	using namespace AMKd::MML::Lexer;		// // //
	if (auto param2 = GetParameters<Sep<'='>, Int>(mml_))
		return param2.get<0>();
	int i = 192 / GetParameters<Option<Int>>(mml_)->value_or(defaultNoteValue);
	int frac = i;

	while (mml_.Trim('.'))
		i += (frac >>= 1);

	if (triplet)
		i = static_cast<int>(i * 2. / 3. + .5);		// // //
	return i;
}

// // //
void SoundEffect::loadMML(const std::string &mml) {
	const auto stat = AMKd::MML::Preprocessor {mml, name};		// // //
	mmlText_ = std::move(stat.result);
	mml_ = AMKd::MML::SourceView {mmlText_};
}

void SoundEffect::compile() {
	triplet = false;
	defaultNoteValue = 8;

	while (mml_.HasNextToken()) {
		try {
			parseStep();		// // //
		}
		catch (AMKd::Utility::MMLException &e) {		// // //
			::printError(e.what(), name, mml_.GetLineNumber());
		}
	}
	if (add0)		// // //
		append(AMKd::Binary::CmdType::End);
	compileASM();
}

void SoundEffect::parseStep() {
#define error(str) { \
		::printError(str, name, mml_.GetLineNumber()); \
		return; \
	}

	// unsigned int instr = -1;
	int lastNote = -1;
	bool firstNote = true;
	bool pitchSlide = false;
	int octave = 4;
	int lastNoteValue = -1;
	int volume[2] = {0x7F, 0x7F};
	bool updateVolume = false;

	using namespace AMKd::MML::Lexer;		// // //
	char ch = (*mml_.Trim("."))[0];
	switch (ch) {
	case ';':
		(void)GetParameters<Row>(mml_);		// // //
		break;

	case '#':
		mml_.SkipSpaces();		// // //
		if (auto param = GetParameters<String>(mml_)) {
			if (param.get<0>() == "asm")
				return parseASM();
			if (param.get<0>() == "jsr")
				return parseJSR();
		}
		throw AMKd::Utility::ParamException {"Unknown option type for '#' directive."};
	case '!':
		mml_.Clear();		// // //
		return;
	case 'v':
		if (auto param = GetParameters<Int>(mml_)) {
			auto vol = requires(param.get<0>(), 0u, 0x7Fu,
				"Illegal value for volume command.  Only values between 0 and 127 are allowed.");
			if (auto extra = GetParameters<Comma, Int>(mml_)) {
				volume[0] = vol;
				volume[1] = requires(extra.get<0>(), 0u, 0x7Fu,
					"Illegal value for volume command.  Only values between 0 and 127 are allowed.");
			}
			else
				volume[0] = volume[1] = vol;
			updateVolume = true;
			return;
		}
		throw AMKd::Utility::SyntaxException {"Error parsing volume command."};
	case 'l':
		if (auto param = GetParameters<Int>(mml_)) {
			defaultNoteValue = requires(param.get<0>(), 1u, 192u, "Illegal value for 'l' directive.");
			return;
		}
		throw AMKd::Utility::SyntaxException {"Error parsing 'l' directive."};
	case '@':
		if (auto param = GetParameters<Int>(mml_)) {
			auto inst = requires(param.get<0>(), 0u, 0x7Fu,
				"Illegal value for instrument ('@') command.");
			if (auto extra = GetParameters<Comma, Int>(mml_)) {
				auto noise = requires(extra.get<0>(), 0u, 0x1Fu,
					"Illegal value for noise instrument ('@,') command.  Only values between 0 and 31 are allowed.");
				append(AMKd::Binary::CmdType::Inst, noise | 0x80, inst);
			}
			else
				append(AMKd::Binary::CmdType::Inst, inst);
			// instr = inst;
			return;
		}
		throw AMKd::Utility::SyntaxException {"Error parsing instrument ('@') command."};
	case 'o':
		if (auto param = GetParameters<Int>(mml_)) {
			octave = requires(param.get<0>(), 1u, 6u, "Illegal value for octave directive.");
			return;
		}
		throw AMKd::Utility::SyntaxException {"Error parsing octave directive."};
	case '$':
		mml_.Unput();		// // //
		if (auto param = GetParameters<Byte>(mml_))
			return append(param.get<0>());
		throw AMKd::Utility::SyntaxException {"Error parsing hex command."};
	case '>':
		octave = requires(octave + 1, 0, 6, "Illegal octave reached via '>' directive.");
		return;
	case '<':
		octave = requires(octave - 1, 0, 6, "Illegal octave reached via '<' directive.");
		return;
	case '{':
		if (std::exchange(triplet, true))
			throw AMKd::Utility::ParamException {"Triplet block mismatch."};
		return;
	case '}':
		if (!std::exchange(triplet, false))
			throw AMKd::Utility::ParamException {"Triplet block mismatch."};
		return;
	case 'a': case 'b': case 'c': case 'd': case 'e': case 'f': case 'g': case 'r': case '^':
	{
		const auto checkVolume = [&] {		// // //
			if (std::exchange(updateVolume, false)) {
				append(volume[0]);
				if (volume[0] != volume[1])
					append(volume[1]);
			}
		};

		int i = ch == 'r' ? AMKd::Binary::CmdType::Rest :
			ch == '^' ? AMKd::Binary::CmdType::Tie : getPitch(ch, octave);
		int len = getNoteLength();

		if (i == lastNoteValue && !updateVolume)
			i = 0;

		if (i != AMKd::Binary::CmdType::Tie && i != AMKd::Binary::CmdType::Rest && (mml_.Trim('&') || pitchSlide)) {
			pitchSlide = true;
			if (firstNote && lastNote == -1)
				lastNote = i;
			else {
				if (len > 0) {
					append(len);
					lastNoteValue = len;
				}

				checkVolume();

				if (std::exchange(firstNote, false))
					append(AMKd::Binary::CmdType::Portamento, lastNote, 0x00, lastNoteValue, i);		// // //
				else
					append(AMKd::Binary::CmdType::BendAway, 0x00, lastNoteValue, i);
			}

			if (len < 0)
				lastNoteValue = len;
			break;
		}
		else {
			firstNote = true;
			pitchSlide = false;
		}

		if (len >= 0x80) {
			append(0x7F);
			checkVolume();
			append(i);

			len -= 0x7F;

			while (len > 0x7F) {
				len -= 0x7F;
				append(AMKd::Binary::CmdType::Tie);
			}

			if (len > 0) {
				if (len != 0x7F) append(len);
				append(AMKd::Binary::CmdType::Tie);
			}

			lastNoteValue = len;
		}
		else if (len > 0) {
			append(len);
			lastNoteValue = len;
			checkVolume();
			append(i);
		}
		else
			append(i);
		return;
	}

	// Phew...
	default:
		printWarning(std::string("Warning: Unexpected symbol '") + ch + std::string("'found."), name, mml_.GetLineNumber());		// // //
	}

#undef error
}

void SoundEffect::parseASM() {
	using namespace AMKd::MML::Lexer;		// // //

	if (mml_.SkipSpaces())
		if (auto param = GetParameters<String, Sep<'{'>>(mml_))
			if (auto asmstr = mml_.TrimUntil("\\}"))
				return (void)asmInfo.emplace(std::move(param.get<0>()), *asmstr);		// // //

	throw AMKd::Utility::SyntaxException {"Error parsing asm directive."};		// // //
}

void SoundEffect::compileASM() {
	//int codeSize = 0;

	std::map<const std::string *, size_t> codePositions;		// // //

	for (const auto &inf : asmInfo) {
		codePositions.emplace(&inf.first, code.size());

		removeFile("temp.bin");
		removeFile("temp.asm");
		writeTextFile("temp.asm", [&] {
			std::stringstream asmCode;
			asmCode << "arch spc700-raw\n\norg $000000\nincsrc \"" << "asm/main.asm" << "\"\nbase $" <<
				hex4 << posInARAM + code.size() + dataStream.GetSize() << "\n\norg $008000\n\n" << inf.second;		// // //
			return asmCode.str();
		});

		if (!asarCompileToBIN("temp.asm", "temp.bin")) {
			printError("asar reported an error.  Refer to temp.log for details.");
			return;
		}

		std::vector<uint8_t> temp = openFile("temp.bin");		// // //
		code.insert(code.cend(), temp.cbegin() + 0x8000, temp.cend());
	}

	for (const auto &inf : asmInfo) {		// // //
		auto it = std::find_if(jmpInfo.cbegin(), jmpInfo.cend(), [&] (const auto &jump) { return jump.first == inf.first; });
		if (it == jmpInfo.cend()) {
			printError("Could not match asm and jsr names.");
			return;
		}
		int k = std::distance(jmpInfo.cbegin(), it);
		auto codePos = posInARAM + dataStream.GetSize() + codePositions[&inf.first];
		assign_short(dataStream.GetData() + jmpInfo[k].second, codePos);		// // //
	}
}

void SoundEffect::parseJSR() {
	using namespace AMKd::MML::Lexer;		// // //

	if (mml_.SkipSpaces())
		if (auto param = GetParameters<String>(mml_)) {
			jmpInfo.emplace_back(std::move(param.get<0>()), dataStream.GetSize() + 1);		// // //
			return append(0xFD, 0x00, 0x00);
		}

	throw AMKd::Utility::SyntaxException {"Error parsing jsr command."};		// // //
}

// // //
