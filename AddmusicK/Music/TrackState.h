#pragma once

namespace AMKd::Music {

class TrackState		// // //
{
public:
	explicit TrackState(int val = 0);

	int Get() const;
	void Update();
	bool NeedsUpdate() const;
	TrackState &operator=(int val);
	TrackState &operator=(const TrackState &) = default;

private:
	int val_ = 0;
	mutable bool update_ = true;
};

} // namespace AMKd::Music
