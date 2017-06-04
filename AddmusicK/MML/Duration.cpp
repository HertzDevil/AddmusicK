#include "Duration.h"
#include "../Utility/Exception.h"

using namespace AMKd::MML;

const int Duration::WHOLE_NOTE_TICKS = 192;
const double Duration::EPSILON = 1e-7;

double Duration::GetTicks() const {
	return mult_ == 0. ? tick_ : throw AMKd::Utility::SyntaxException {"A default note length cannot be used here."};
}

double Duration::GetTicks(int defaultLen) const {
	return WHOLE_NOTE_TICKS / defaultLen * mult_ + tick_;
}

double Duration::GetLastTicks(int defaultLen) const {
	return WHOLE_NOTE_TICKS / defaultLen * lastMult_ + lastTick_;
}
