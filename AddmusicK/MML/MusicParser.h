#pragma once

class Music;

namespace AMKd::MML {

class SourceView;

class MusicParser
{
public:
	void compile(SourceView &file, ::Music &music);

private:
	void parseComment(SourceView &file, ::Music &music);
	void parseReplacementDirective(SourceView &file, ::Music &music);
};

} // namespace AMKd::MML
