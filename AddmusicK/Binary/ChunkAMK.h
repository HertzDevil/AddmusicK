#pragma once

#include "Chunk.h"
#include "Defines.h"

// // // it is possible to remove Defines.h completely but for now leave it as is

namespace AMKd::Binary {

namespace ChunkNSPC {

using End = ChunkTemplate<0, CmdType::End>;
using Vibrato = ChunkTemplate<3, CmdType::Vibrato>;

} // namespace NSPC

namespace ChunkAMK {

using RestoreInst = ChunkTemplate<0, CmdType::ExtF4, CmdOptionF4::RestoreInst>;
using Gain = ChunkTemplate<1, CmdType::ExtFA, 0x01>;
using RemoteCall = ChunkTemplate<4, CmdType::Callback>;

} // namespace AMK

} // namespace AMKd::Binary
