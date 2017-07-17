#pragma once

#include "Chunk.h"
#include "Defines.h"

// // // it is possible to remove Defines.h completely but for now leave it as is

namespace AMKd::Binary {

namespace ChunkNSPC {

using End = ChunkTemplate<0, CmdType::End>;
using Tie = ChunkTemplate<0, CmdType::Tie>;
using Rest = ChunkTemplate<0, CmdType::Tie>;
using Inst = ChunkTemplate<1, CmdType::Inst>;
using Pan = ChunkTemplate<1, CmdType::Pan>;
using PanFade = ChunkTemplate<2, CmdType::PanFade>;
using Portamento = ChunkTemplate<3, CmdType::Portamento>;
using Vibrato = ChunkTemplate<3, CmdType::Vibrato>;
using VibratoOff = ChunkTemplate<0, CmdType::VibratoOff>;
using VolGlobal = ChunkTemplate<1, CmdType::VolGlobal>;
using VolGlobalFade = ChunkTemplate<2, CmdType::VolGlobalFade>;
using Tempo = ChunkTemplate<1, CmdType::Tempo>;
using TempoFade = ChunkTemplate<2, CmdType::TempoFade>;
using TrspGlobal = ChunkTemplate<1, CmdType::TrspGlobal>;
using Tremolo = ChunkTemplate<3, CmdType::Tremolo>;
//using TremoloOff = ChunkTemplate<0, CmdType::Tremolo + 1>;
using Vol = ChunkTemplate<1, CmdType::Vol>;
using VolFade = ChunkTemplate<2, CmdType::VolFade>;
using Loop = ChunkTemplate<3, CmdType::Loop>;
using VibratoFade = ChunkTemplate<1, CmdType::VibratoFade>;
using BendAway = ChunkTemplate<3, CmdType::BendAway>;
using BendTo = ChunkTemplate<3, CmdType::BendTo>;
using Detune = ChunkTemplate<1, CmdType::Detune>;
using EchoVol = ChunkTemplate<3, CmdType::EchoVol>;
using EchoOff = ChunkTemplate<0, CmdType::EchoOff>;
using EchoParams = ChunkTemplate<3, CmdType::EchoParams>;
using EchoFade = ChunkTemplate<3, CmdType::EchoFade>;

} // namespace NSPC

namespace ChunkAMK {

using Subloop = ChunkTemplate<1, CmdType::Subloop>;
using Envelope = ChunkTemplate<2, CmdType::Envelope>;
using SampleLoad = ChunkTemplate<2, CmdType::SampleLoad>;
using YoshiCh5 = ChunkTemplate<0, CmdType::ExtF4, CmdOptionF4::YoshiCh5>;
using Legato = ChunkTemplate<0, CmdType::ExtF4, CmdOptionF4::Legato>;
using LightStaccato = ChunkTemplate<0, CmdType::ExtF4, CmdOptionF4::LightStaccato>;
using EchoToggle = ChunkTemplate<0, CmdType::ExtF4, CmdOptionF4::EchoToggle>;
using Sync = ChunkTemplate<0, CmdType::ExtF4, CmdOptionF4::Sync>;
using Yoshi = ChunkTemplate<0, CmdType::ExtF4, CmdOptionF4::Yoshi>;
using TempoImmunity = ChunkTemplate<0, CmdType::ExtF4, CmdOptionF4::TempoImmunity>;
using Louder = ChunkTemplate<0, CmdType::ExtF4, CmdOptionF4::Louder>;
using RestoreInst = ChunkTemplate<0, CmdType::ExtF4, CmdOptionF4::RestoreInst>;
using FIR = ChunkTemplate<8, CmdType::FIR>;
using DSP = ChunkTemplate<2, CmdType::DSP>;
using ARAM = ChunkTemplate<3, CmdType::ARAM>;
using Noise = ChunkTemplate<1, CmdType::Noise>;
using DataSend = ChunkTemplate<2, CmdType::DataSend>;
using PitchMod = ChunkTemplate<1, CmdType::ExtFA, CmdOptionFA::PitchMod>;
using Gain = ChunkTemplate<1, CmdType::ExtFA, CmdOptionFA::Gain>;
using Transpose = ChunkTemplate<1, CmdType::ExtFA, CmdOptionFA::Transpose>;
using Amplify = ChunkTemplate<1, CmdType::ExtFA, CmdOptionFA::Amplify>;
using EchoBuffer = ChunkTemplate<1, CmdType::ExtFA, CmdOptionFA::EchoBuffer>;
using GainOld = ChunkTemplate<1, CmdType::ExtFA, CmdOptionFA::GainOld>;
using VolTable = ChunkTemplate<1, CmdType::ExtFA, CmdOptionFA::VolTable>;
using Arpeggio = ChunkTemplate<2, CmdType::Arpeggio>; // the arp sequence will use a separate chunk
using Trill = ChunkTemplate<2, CmdType::Arpeggio, ArpOption::Trill>;
using Glissando = ChunkTemplate<2, CmdType::Arpeggio, ArpOption::Glissando>;
using RemoteCall = ChunkTemplate<4, CmdType::Callback>;

} // namespace AMK

} // namespace AMKd::Binary
