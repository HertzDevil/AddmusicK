#pragma once

#include <vector>
#include <cstdint>
#include <algorithm>
#include <type_traits>

namespace AMKd::Binary {

class IChunk;

class Stream
{
public:
	void Flush(std::vector<uint8_t> &buf) const;
	void Flush(Stream &other) const;

	template <typename T, std::enable_if_t<std::is_integral_v<T>, bool> = 0>
	Stream &operator<<(T x) & {
		data_.push_back(static_cast<uint8_t>(x));
		return *this;
	}
	template <typename T, std::enable_if_t<std::is_integral_v<T>, bool> = 0>
	Stream &&operator<<(T x) && {
		data_.push_back(static_cast<uint8_t>(x));
		return std::move(*this);
	}

	Stream &operator<<(const IChunk &c);

	size_t GetSize() const;
	uint8_t *GetData();
	const uint8_t *GetData() const;

private:
	std::vector<uint8_t> data_;
};

} // namespace AMKd::Binary
