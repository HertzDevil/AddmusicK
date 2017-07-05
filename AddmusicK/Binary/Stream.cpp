#include "Stream.h"
#include "Chunk.h"

using namespace AMKd::Binary;

void Stream::Flush(std::vector<uint8_t> &buf) const {
	buf.insert(cend(buf), cbegin(data_), cend(data_));
}

void Stream::Flush(Stream &other) const {
	Flush(other.data_);
}

Stream &Stream::operator<<(const IChunk &c) {
	c.Insert(*this);
	return *this;
}

size_t Stream::GetSize() const {
	return size(data_);
}

uint8_t *Stream::GetData() {
	return data(data_);
}

const uint8_t *Stream::GetData() const {
	return data(data_);
}
