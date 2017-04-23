# AddmusicK

You saw this coming!

This repository is based on AddmusicK version 1.0.4. (I don't think the parser ever changed in 1.0.5.) Changes to existing code are marked with `// // //`; new things should all be present in the `AMKd` namespace. "AMKd" is not the name of this fork; there is no name yet.

Assume that only AddmusicK will be modified and the 4 other projects in the solution won't.

#### Programming stuffs

- [x] Replace `boost::filesystem` with `std::experimental::filesystem`
- [ ] Remove VS-exclusive bloat
- [ ] Decompose pretty much everything in `Music.cpp`
- [ ] Bring in the `Chunk` classes I wrote a while ago (originally for 0CC-FT)
- [ ] Makefiles (maybe)

#### Compiler stuffs

- [ ] Run AMK from anywhere as long as it is available in `$PATH`
- [ ] Conditional compilation for both sound driver and music data
- [ ] Makefile / Ruby script for inserting music into SMW
- [ ] If we could achieve that, we could also make the core compiler platform-independent

#### MML stuffs

- [ ] Separate `o`/`l`/`h`/`q` states for each track
- [ ] Key signature and full accidental support
- [ ] Legible names for all commands
- [ ] Track multiplexing (e.g. `#0123`)
- [ ] N-SPC block support
- [ ] Globally defined patterns

#### Things that should die (and might irreversibly break some MMLs)

- [ ] Hex validation, there is no reason to once all commands have names
- [ ] Lexical substitution macros, they must occur at the token level
- [ ] Whitespace requirements, missing post-conditions, case-insensitivity etc.
- [ ] If this really gets used by people in the future, drop support for everything below `#amk 2`

#### Things that already died

- `|` command used for bypassing hex validation, it is now a no-op (since there is no guarantee bytes already placed with `$` will be inserted verbatim)
