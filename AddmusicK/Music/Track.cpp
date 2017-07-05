#include "Track.h"
#include "../globals.h"
#include "../Binary/Defines.h"

using namespace AMKd::Music;

size_t Track::GetStreamSize() const {
	return data.size();
}

bool Track::HasData() const {
	return !data.empty();
}

void Track::FlushData(std::vector<uint8_t> &bin) const {
	bin.insert(bin.cend(), data.cbegin(), data.cend());
}

void Track::AddLoopPosition(size_t offset) {
	loopLocations.push_back(static_cast<uint16_t>(GetStreamSize() + offset));
}

void Track::AddRemoteConversion(uint8_t cmdtype, uint8_t param, std::vector<uint8_t> &&cmd) {
	remoteGainInfo.emplace_back(static_cast<uint16_t>(GetStreamSize() + 1), std::move(cmd));		// // //
	append(AMKd::Binary::CmdType::Callback, 0x00, 0x00, cmdtype, param);		// // //
}

void Track::ShiftPointers(int offset) {
	for (unsigned short x : loopLocations)
		assign_short(data.begin() + x, ((data[x] & 0xFF) | (data[x + 1] << 8)) + offset);		// // //
}

void Track::InsertRemoteCalls(Track &loop) {
	for (const auto &x : remoteGainInfo) {		// // //
		loopLocations.push_back(x.first);
		assign_short(data.begin() + x.first, loop.data.size());
		loop.data.insert(loop.data.end(), x.second.cbegin(), x.second.cend());
	}
}
