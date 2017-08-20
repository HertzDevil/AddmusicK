#pragma once

#include <vector>
#include <cstdint>
#include <utility>
#include "TrackState.h"
#include "../Utility/Swallow.h"
#include "../Binary/Stream.h"

class Music;

namespace AMKd::Music {

class Track
{
public:
	size_t GetStreamSize() const;
	bool HasData() const;
	void FlushData(std::vector<uint8_t> &bin) const;

	void AddLoopPosition(size_t offset);
	void AddRemoteConversion(uint8_t cmdtype, uint8_t param, const AMKd::Binary::Stream &cmd);
	void AddRemoteConversion(uint8_t cmdtype, uint8_t param, AMKd::Binary::Stream &&cmd);

	void ShiftPointers(int offset);
	void InsertRemoteCalls(Track &loop);

	void Append(const AMKd::Binary::IChunk &chunk);		// // //
	template <typename... Args>
	void append(Args&&... value) {
#if __cplusplus >= 201703L
		(data.push_back(static_cast<uint8_t>(value)), ...);
#else
		SWALLOW(streamData_ << static_cast<uint8_t>(value));
#endif
	}

private:
	AMKd::Binary::Stream streamData_;		// // //
	std::vector<uint16_t> loopLocations; // With remote loops, we can have remote loops in standard loops, so we need that ninth channel.
	std::vector<std::pair<uint16_t, AMKd::Binary::Stream>> remoteGainInfo;		// // // holds position and remote call data for gain command conversions

public:
	int index = 0;

	double channelLength = 0.; // How many ticks are in each channel.
	TrackState q {0x7F};		// // //
	TrackState o {4};		// // //
	TrackState l {8};		// // //
	uint8_t lastDuration = 0;		// // // replaces prevNoteLength
	int8_t h = 0;		// // //
	bool usingH = false;		// // //
	int instrument = 0;
	//uint8_t lastFAGainValue = 0;
	//uint8_t lastFADelayValue = 0;
	//uint8_t lastFCGainValue = 0;
	uint8_t lastFCDelayValue = 0;

	unsigned short phrasePointers[2] = {0, 0}; // first 8 only
	bool noMusic[2] = {false, false}; // first 8 only
	bool passedIntro = false; // first 8 only
	bool usingFA = false;
	bool usingFC = false;
	bool ignoreTuning = false; // Used for AM4 compatibility.  Until an instrument is explicitly declared on a channel, it must not use tuning.
	bool isDefiningLabelLoop = false;		// // //
	bool inPitchSlide = false;		// // //
	bool inTriplet = false;		// // //
};

} // namespace AMKd::Music
