#include "TrackState.h"
#include <utility>

using namespace AMKd::Music;

TrackState::TrackState(int val) : val_(val)
{
}

int TrackState::Get() const {
	return val_;
}

void TrackState::Update() {
	update_ = true;
}

bool TrackState::NeedsUpdate() const {
	return std::exchange(update_, false);
}

TrackState &TrackState::operator=(int val) {
//	if (val != val_)
		Update();
	val_ = val;
	return *this;
}
