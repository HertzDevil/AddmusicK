#pragma once

#include <cstdint>

#define SCOPED_ENUM_START(T, U) \
	namespace T { \
		enum : U {

#define SCOPED_ENUM_END() \
		}; \
	}

namespace AMKd::Binary {

SCOPED_ENUM_START(CmdType, uint8_t)
	End           = 0x00,
	Tie           = 0xC6,
	Rest          = 0xC7,
	Inst          = 0xDA,
	Pan           = 0xDB,
	PanFade       = 0xDC,
	Portamento    = 0xDD,
	Vibrato       = 0xDE,
	VibratoOff    = 0xDF,
	VolGlobal     = 0xE0,
	VolGlobalFade = 0xE1,
	Tempo         = 0xE2,
	TempoFade     = 0xE3,
	TrspGlobal    = 0xE4,
	Tremolo       = 0xE5,
	Subloop       = 0xE6,
	Vol           = 0xE7,
	VolFade       = 0xE8,
	Loop          = 0xE9,
	VibratoFade   = 0xEA,
	BendAway      = 0xEB,
	BendTo        = 0xEC,
	Envelope      = 0xED,
	Detune        = 0xEE,
	EchoVol       = 0xEF,
	EchoOff       = 0xF0,
	EchoParams    = 0xF1,
	EchoFade      = 0xF2,
	SampleLoad    = 0xF3,
	ExtF4         = 0xF4,
	FIR           = 0xF5,
	DSP           = 0xF6,
	ARAM          = 0xF7,
	Noise         = 0xF8,
	DataSend      = 0xF9,
	ExtFA         = 0xFA,
	Arpeggio      = 0xFB,
	Callback      = 0xFC,
SCOPED_ENUM_END()

SCOPED_ENUM_START(CmdOptionF4, uint8_t)
	YoshiCh5      = 0x00,
	Legato        = 0x01,
	LightStaccato = 0x02,
	EchoToggle    = 0x03,
	Sync          = 0x05,
	Yoshi         = 0x06,
	TempoImmunity = 0x07,
	Louder        = 0x08,
	RestoreInst   = 0x09,
SCOPED_ENUM_END()

SCOPED_ENUM_START(CmdOptionFA, uint8_t)
	PitchMod   = 0x00,
	Gain       = 0x01,
	Transpose  = 0x02,
	Amplify    = 0x03,
	EchoBuffer = 0x04,
	GainOld    = 0x05,
	VolTable   = 0x06,		// // // not in manual
SCOPED_ENUM_END()

SCOPED_ENUM_START(CmdOptionFC, uint8_t)
	Disable    = 0x00,
	Sustain    = 0x01,
	Release    = 0x02,
	KeyOff     = 0x03,
	Once       = 0x04,
	KeyOffConv = 0x05,
	KeyOn      = 0xFF,
SCOPED_ENUM_END()

SCOPED_ENUM_START(ArpOption, uint8_t)
	Trill     = 0x80,
	Glissando = 0x81,
SCOPED_ENUM_END()

} // namespace AMKd::Binary

#undef SCOPED_ENUM_START
#undef SCOPED_ENUM_END
