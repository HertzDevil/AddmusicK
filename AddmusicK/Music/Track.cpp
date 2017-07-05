#include "Track.h"
#include "../globals.h"
#include "../Binary/ChunkAMK.h"

using namespace AMKd::Music;

size_t Track::GetStreamSize() const {
	return streamData_.GetSize();
}

bool Track::HasData() const {
	return GetStreamSize() != 0;
}

void Track::FlushData(std::vector<uint8_t> &bin) const {
	streamData_.Flush(bin);
}

void Track::AddLoopPosition(size_t offset) {
	loopLocations.push_back(static_cast<uint16_t>(GetStreamSize() + offset));
}

void Track::AddRemoteConversion(uint8_t cmdtype, uint8_t param, const AMKd::Binary::Stream &cmd) {
	remoteGainInfo.emplace_back(static_cast<uint16_t>(GetStreamSize() + 1), cmd);		// // //
	streamData_ << AMKd::Binary::ChunkAMK::RemoteCall(0x00, 0x00, cmdtype, param);
}

void Track::AddRemoteConversion(uint8_t cmdtype, uint8_t param, AMKd::Binary::Stream &&cmd) {
	remoteGainInfo.emplace_back(static_cast<uint16_t>(GetStreamSize() + 1), std::move(cmd));		// // //
	streamData_ << AMKd::Binary::ChunkAMK::RemoteCall(0x00, 0x00, cmdtype, param);
}

void Track::ShiftPointers(int offset) {
	auto ptr = streamData_.GetData();
	for (unsigned short x : loopLocations)
		assign_short(ptr + x, ((ptr[x] & 0xFF) | (ptr[x + 1] << 8)) + offset);		// // //
}

void Track::InsertRemoteCalls(Track &loop) {
	for (const auto &x : remoteGainInfo) {		// // //
		loopLocations.push_back(x.first);
		assign_short(streamData_.GetData() + x.first, loop.GetStreamSize());
		x.second.Flush(loop.streamData_);
	}
}
