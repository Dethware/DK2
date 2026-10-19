# Dungeon Keeper II sound data formats

This page describes the files behind DK II's background sound: the music, the ambience and the
atmospheric one-shots. The general sound effects use the same formats.

All values are little endian. Offsets are hexadecimal. "Unknown" means the field was looked at
but its purpose is not established. The readers in [`tools/`](../tools) (Python) and
[`player/src/core/`](../player/src/core) (C++) implement everything described here, and the
readers check that every byte of every background map is accounted for.

The game keeps its sound in `Data/Sound/Sfx/`:

- `Global/` holds the shared categories. Each category is a `<category>BANK.map` and
  `<category>SFX.map` pair, e.g. `musicBANK.map` + `musicSFX.map`.
- Each `<name>HD.sdt` / `<name>HW.sdt` archive holds the audio. The game prefers the `HD`
  (software mixed) variant. All the background archives exist only as `HD`.
- Level speech lives in the `speech_*` folders, which are not covered here.

On case-sensitive file systems note the real spelling: `Data/Sound/Sfx`, while the game's own
strings say `sfx`.

## `.sdt`: sound archive

```
u32  count
u32  offsets[count]          absolute offset of each entry header
then per entry:
  u32  headerSize            always 0x28
  u32  dataSize
  char name[16]              NUL padded, e.g. "1pt1-001.mp2"
  u16  sampleRate            22050
  u8   bitsPerSample         16
  u8   type                  0x25 MPEG stereo, 0x24 MPEG mono, 0 empty ("Blank")
  u32  0
  u32  pcmBytes              length of the decoded sound as 16 bit PCM
  u32  0
  u8   data[dataSize]        a raw MPEG audio stream
```

The background audio is MPEG-1/2 audio, at 22050 Hz:

- Music and ambience: MPEG-2 Layer II, 112 kbit/s stereo.
- One-shot atmosphere: MPEG-1 Layer I, mono.

Any MPEG audio decoder handles them, for example FFmpeg.

**`pcmBytes` matters for the music.** An MPEG stream decodes to whole 1152-sample frames, so the
decoder returns a little more than the sound's real length. The game plays exactly `pcmBytes` of
it, counted from the first decoded sample with no decoder-delay trim. Cut that way, consecutive
music segments join sample-exactly. This was checked by measuring the waveform continuity at the
joins: offset 0 is smooth, while offsets of 240-1105 samples produce clear discontinuities.

### The background archives

| Archive | Entries | Length per entry |
|---|---|---|
| `Track_1_1HD` | 46 | 13235 ms (`pcmBytes` 1167316) |
| `Track_1_2HD` | 35 + 1 empty | 13235 ms (1167332) |
| `Track_1_3HD` | 22 | 13235 ms |
| `Track_1_4HD` | 13 | 13235 ms |
| `Track_1SGHD` | 10 | 13235 ms |
| `Track_3_1HD` | 79 | 10663 ms (940452) |
| `Track_3_2HD` | 20 | 10663 ms |
| `Track_3_3HD` | 18 | 10663 ms |
| `Track_3_4HD` | 24 | 10663 ms |
| `Track_ZeroHD` | 21 + 18 empty | 6.7-125.5 s |
| `SoundFontHD` | 19 | 0.4-4.7 s |

`Track_Zero` holds the ambience drones. Despite its name, `SoundFontHD` holds the
atmospheric one-shots: creaks, doors, monster growls, drips and torture sounds.

## `BANK.map`: archive list

```
00  u8   guid[16]            01 2c 61 e9 d0 31 d2 11 b4 09 00 a0 c9 93 f2 03
10  u32  0, 0
18  u32  count
1c  per archive, 11 bytes   runtime data (ff ff ff ff, a pointer, ...), ignore
    per archive:
      u32  length
      char name[length]      NUL terminated, e.g. "Global\Track_1_1" (relative to Data/Sound/Sfx/)
```

A sound's `bank` field (see below) is a 1-based index into this list. Append `HD.sdt` or `HW.sdt` to
get the archive file.

## `SFX.map`: sound groups

The game loads the whole file into memory and turns the indices and pointer fields into pointers
in place. So the runtime structures in `DKII.EXE` have exactly this layout, and the offsets below
are also the ones the code uses.

```
00  u8   guid[16]            00 2c 61 e9 d0 31 d2 11 b4 09 00 b0 c9 93 f2 03
10  u32  0, 0
18  u32  entryCount
1c  Entry   entries[entryCount]                (24 bytes each)
    Group   groups[sum of entry.groupCount]    (20 bytes each)
    then for each group, in order:
      Item  items[group.itemCount]             (42 bytes each)
      then for each item, in order:
        Sound  sounds[item.soundCount]         (16 bytes each)
        Link   links[item.linkCount]           (8 bytes each)
```

The file must end exactly after the last link.

### Entry (24 bytes)

| Offset | Type | Field |
|---|---|---|
| 00 | u32 | groupCount |
| 04 | u32 | unknown (0) |
| 08 | u16, u16 | unknown (3, 1 for music; 9, 1 for ambience) |
| 0c | f32 | min distance (1.0) |
| 10 | f32 | max distance (2.0, or 0.0 for options music) |
| 14 | f32 | scale (0.5) |

The distance names come from OpenKeeper. They are 3D attenuation parameters and play no role in
the background sound.

### Group (20 bytes)

A group is one playable sound: for example a music track, the ambience, or one sound effect.

| Offset | Type | Field |
|---|---|---|
| 00 | u32 | id: the number the game asks for, see below |
| 04 | u32 | itemCount |
| 08 | u32 | runtime: items pointer |
| 0c | u32 | unknown, probably priority (6000 music and ambience, 1000 options music, 149 atmosphere) |
| 10 | u16 | **flags**: they select the player class, see [runtime behaviour](runtime-behaviour.md#player-classes) |
| 12 | u8 | id of parameter slot 0: 2 for music and ambience, which makes slot 0 the **intensity** |
| 13 | u8 | 0 |

| Group | Category | Flags | Items | What it is |
|---|---|---|---|---|
| 343 | `music` | 0x0606 | 135 | music track 1 |
| 345 | `music` | 0x0606 | 156 | music track 3 |
| 341 | `ambience` | 0x0606 | 21 | `Track_Zero` ambience drones |
| 746 | `one_shot_atmos` | 0x0204 | 1 | atmospheric one-shots |
| 838 | `options_music` | 0x021e | 16 | the options screen loop (over `Track_1_4`) |

### Item (42 bytes)

An item is one node of a sequence: for example a single music segment.

| Offset | Type | Field |
|---|---|---|
| 00 | u32 | soundCount |
| 04 | u32 | linkCount |
| 08 | u32 | runtime: sounds pointer |
| 0c | u8 | volume min, percent (music 100, ambience 40, atmosphere 21) |
| 0d | u8 | volume max; the game picks a random volume in [min, max) |
| 0e | s8 | pan min (probably), picked like the volume |
| 0f | s8 | pan max |
| 10 | u16 | **delay min**, ms: start-to-start delay for delay-chained groups (atmosphere 8000, music 0) |
| 12 | u16 | **delay max**, ms (atmosphere 20000) |
| 14 | u8, u8 | unknown (always 50, 50) |
| 16 | u16 | id of parameter slot 1 (0 in all background groups) |
| 18 | u16 | slot 1 use: bit 0 takes the volume from slot 1, bit 1 the pan |
| 1a | u16 | id of parameter slot 2 (0 in all background groups) |
| 1c | u16 | slot 2 use, as above |
| 1e | u16 | cumulative item weight (`65535 * (i + 1) / n`), used by the generic item picker only |
| 20 | u16 | 0 |
| 22 | f32 | unknown (20.0 for most items, 100.0 for some ambience items and the atmosphere, 0.1 for the last three items of track 3) |
| 26 | u32 | runtime: links pointer |

### Sound (16 bytes)

An item plays one of its sounds. Every background item has exactly one sound, except the
atmosphere item, which has 19.

| Offset | Type | Field |
|---|---|---|
| 00 | u32 | entry index in the archive, 1-based |
| 04 | u32 | cumulative weight in the low 16 bits; the first sound with `rand16 <= weight` plays |
| 08 | u32 | duration in ms (rounded `pcmBytes`, e.g. 13235) |
| 0c | u32 | bank: 1-based index into the BANK.map archive list |

### Link (8 bytes)

A link says which item may play next, and at which intensities.

| Offset | Type | Field |
|---|---|---|
| 00 | u32 | target item, 1-based index in the same group (a pointer at runtime) |
| 04 | u8, u8 | 0, 0 |
| 06 | u8 | min intensity, inclusive |
| 07 | u8 | max intensity, inclusive |

Some items list the same target twice in one range. This is how the data weights the random
choice, see [runtime behaviour](runtime-behaviour.md#picking-the-next-item).

## The music graph in numbers

Intensity comes in steps of 20 (see [runtime behaviour](runtime-behaviour.md#the-music-level)).
Each step selects a part of the track:

| Intensity | Track 1 | Track 3 |
|---|---|---|
| 0 (silent) | no links: the music stops | part 4 has links into item 153, probably an outro |
| 20 | part 1 (`Track_1_1`, items 1-52) | part 1 (`Track_3_1`, items 1-87) |
| 40 | part 2 (`Track_1_2`, items 53-87) | part 2 (`Track_3_2`, items 88-110) |
| 60 | part 3 (`Track_1_3`, items 88-109) | part 3 (`Track_3_3`, items 111-128) |
| 80 | part 4 (`Track_1_4`, items 110-125) | part 4 (`Track_3_4`, items 129-156) |

Inside a part the links mostly lead to the next segment, and many segments also have exits to
entry points of the other parts. Some phrases have links for the whole 20-100 range, so they
always play through before the music can change part.

In track 1 the way down from parts 3 and 4 runs through the ten `Track_1SG` transition segments
(items 126-135). Track 3 has no transition bank. Item 1 is the entry point of a track (see
[runtime behaviour](runtime-behaviour.md#starting-stopping-and-resuming)): in track 1 it leads
only to item 2. In track 3 it has 14 targets, and one of them is item 1 itself, so there item 1 can
also play as a normal segment.

The ambience (group 341) is a random walk over its 21 items. Almost all of its links cover
intensities 0-39, so it fades out of the picture once the music level reaches 2.

`tools/sfxmap.py <BANK.map> <SFX.map>` prints a whole graph.
