#pragma once

#include <string_view>

class Music;

namespace AMKd::MML {

class SourceView;

class MusicParser
{
public:
	void operator()(SourceView &file, ::Music &music);
};

namespace Parser {

struct Comment
{
	void operator()(SourceView &file, ::Music &music);
};

struct Replacement
{
	void operator()(SourceView &file, ::Music &music);
};

} // Parser

} // namespace AMKd::MML
