#pragma once

#include <array>
#include <cstdint>
#include "Stream.h"
#include "../Utility/Swallow.h"

namespace AMKd::Binary {

class Stream;

class IChunk
{
public:
	virtual ~IChunk() noexcept = default;
	virtual void Insert(Stream &) const = 0;
};

template <size_t N, uint8_t... Prefix>
class ChunkTemplate : public IChunk
{
public:
	template <typename... Ts>
	ChunkTemplate(Ts... xs) : suffix_ {static_cast<uint8_t>(xs)...} { }

private:
	void Insert(Stream &s) const override {
		SWALLOW(s << Prefix);
		for (uint8_t x : suffix_)
			s << x;
	}

	std::array<uint8_t, N> suffix_;
};

template <size_t N>
using ByteChunk = ChunkTemplate<N>;

template <typename... Ts>
auto MakeByteChunk(Ts&&... xs) {
	return ByteChunk<sizeof...(Ts)>(static_cast<uint8_t>(xs)...);
}

class ListChunk : public IChunk
{
public:
	ListChunk(const std::vector<uint8_t> &data) : data_ {data} { }
	ListChunk(std::vector<uint8_t> &&data) : data_ {std::move(data)} { }

private:
	void Insert(Stream &s) const override {
		for (uint8_t x : data_)
			s << x;
	}

	std::vector<uint8_t> data_;
};

} // namespace AMKd::Binary
