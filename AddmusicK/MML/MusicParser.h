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

#define PARSER_DECL(T) \
	struct T \
	{ \
		void operator()(SourceView &file, ::Music &music); \
	};

PARSER_DECL(Comment)
PARSER_DECL(ExMark)
PARSER_DECL(Bar)
PARSER_DECL(Replacement)
PARSER_DECL(RaiseOctave)
PARSER_DECL(LowerOctave)
PARSER_DECL(LoopBegin)
PARSER_DECL(SubloopBegin)
PARSER_DECL(ErrorBegin)
PARSER_DECL(ErrorEnd)
PARSER_DECL(Intro)
PARSER_DECL(TripletOpen)
PARSER_DECL(TripletClose)
//;

#undef PARSER_DECL

} // Parser

} // namespace AMKd::MML
