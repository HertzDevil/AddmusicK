#include "Track.h"
#include "../globals.h"

using namespace AMKd::Music;

void Track::shiftPointers(int offset) {
	for (unsigned short x : loopLocations)
		assign_short(data.begin() + x, ((data[x] & 0xFF) | (data[x + 1] << 8)) + offset);		// // //
}

void Track::insertRemoteCalls(Track &loop) {
	for (const auto &x : remoteGainInfo) {		// // //
		loopLocations.push_back(x.first);
		assign_short(data.begin() + x.first, loop.data.size());
		loop.data.insert(loop.data.end(), x.second.cbegin(), x.second.cend());
	}
}
