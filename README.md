# Dungeon Keeper II dynamic music

Dungeon Keeper II (Bullfrog, 1999) has adaptive music. The tracks are cut into short segments,
and the game strings them together depending on how intense the fighting in your dungeon is.
This project reverse engineers how that works and plays it back outside the game.

- [`docs/`](docs): the reverse-engineering documentation.
  - [File formats](docs/file-formats.md): the `.sdt` sound archives, `BANK.map` and `SFX.map`, which
    hold the music graph.
  - [Runtime behaviour](docs/runtime-behaviour.md): how the game picks segments, the music level,
    the battle threat score and the Horned Reaper. Every point cites its `DKII.EXE` address.
  - [Open questions](docs/open-questions.md): what is still unknown.
- [`player/`](player): a standalone player (Qt 6, C++) that plays the music, ambience and
  atmosphere layers like the game does, with a slider for the battle threat.
- [`tools/`](tools): Python readers for the data files, and Ghidra scripts used for the analysis.

The same logic is being contributed to [OpenKeeper](https://github.com/tonihele/OpenKeeper),
the open source DK II engine remake.

You need your own copy of Dungeon Keeper II, for example from Steam or GOG. No game data is
included here.

## The short version

- The two music tracks are cut into 13.2 s (track 1) and 10.7 s (track 3) segments. Each segment
  lists which segments may follow it, and at which **intensity**.
- The intensity comes from a **music level**. Fights request level 1. Battles request levels 2-4
  when the summed threat of the enemies reaches 100, 500 or 1500. A Horned Reaper requests level 4.
- After each game tick the level falls back to 1, so the music calms down once the fighting
  stops, but it keeps playing. It is silent only from the level start until the first fight.
- The music reads the intensity only at segment boundaries. The next segment is picked uniformly
  among the matching links; repeating a link in the data makes it likelier.
- An ambience drone and randomly spaced atmospheric one-shots play underneath.

## The player

Requirements: Qt 6.8 or newer with Qt Multimedia (FFmpeg backend), CMake 3.21 and a C++20 compiler.

```bash
cmake -S player -B player/build -G Ninja -DCMAKE_BUILD_TYPE=Release
```

```bash
cmake --build player/build
```

```bash
player/build/src/app/dk2music "/path/to/Dungeon Keeper 2"
```

Pick a track and press Play. The music stays silent, as at a level start, until you move the
**battle threat** slider above 0, which stands for a fight. Higher values raise the music level at
the thresholds read from the game data. "Reset to silent" goes back to the level start state.

The tests check the parser and the sequencing logic. With game data they also decode and render
90 seconds of track 1 and check every transition and segment join:

```bash
DK2_GAME_DIR="/path/to/Dungeon Keeper 2" player/build/tests/tst_dk2music
```

The code is split so the logic can be reused without the UI:

- `player/src/core`:
  - `SdtArchive`, `soundmaps` and `GameData` read the data.
  - `Sequencer` and `MusicLevel` are the game's rules.
  - `ClipCache` decodes with `QAudioDecoder`.
  - `Layer` and `Mixer` produce the audio for a `QAudioSink`.
  - `Engine` ties it together.
- `player/src/app`: the window, which only binds widgets to `Engine`.

## Tools

The Python tools need only the standard library:

```bash
python3 tools/sdt.py -x extracted "/path/to/Dungeon Keeper 2/Data/Sound/Sfx/Global/Track_1_1HD.sdt"
```

```bash
python3 tools/sfxmap.py "$G/musicBANK.map" "$G/musicSFX.map"
```

- `sdt.py` lists an archive's entries, and with `-x` extracts the MPEG streams.
- `sfxmap.py` prints a sound category's groups, items, sounds and links. In the example above,
  `G` is the game's `Data/Sound/Sfx/Global` folder.

For the Ghidra scripts, analyze `DKII.EXE` once with Ghidra's `analyzeHeadless`, then run them
against that project:

```bash
"$GHIDRA_INSTALL_DIR/support/analyzeHeadless" "$GHIDRA_PROJ" DK2 -import "/path/to/Dungeon Keeper 2/DKII.EXE"
```

```bash
tools/ghidra/run.sh Decompile.java depth=1 0x006163c0
```

- `Decompile.java [depth=N] <address>...` decompiles functions and, with `depth`, their callees.
- `FindRefs.java ref:<addr> | scalar:<value> | str:<text>` finds references.
- `FindDisp.java +0x26 +0x6 ...` lists functions that use all the given struct offsets.
