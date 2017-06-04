#pragma once

// // // duration representation

namespace AMKd::MML {

struct Duration
{
	double GetTicks() const;
	double GetTicks(int defaultLen) const;
	double GetLastTicks(int defaultLen) const;

	static const int WHOLE_NOTE_TICKS;
	static const double EPSILON;

	double mult_ = 0.0; // l4 c
	double tick_ = 0.0; // c=1
	double lastMult_ = 0.0;
	double lastTick_ = 0.0;
};

} // namespace AMKd::MML
