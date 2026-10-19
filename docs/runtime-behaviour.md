# How Dungeon Keeper II plays its dynamic music

This page explains how the game decides what to play, from fights in the dungeon down to the next
13-second music segment. The data structures are described in [file formats](file-formats.md).

## Sources and how to check them

Everything here comes from static analysis of the game executable, unless marked otherwise:

| | |
|---|---|
| Binary | `DKII.EXE` of the Steam release, SHA-256 `b5e4e2f6b76335e2a2e7f43bb6903bfc6f4170eabf04598955b00f512a47b449` |
| Tool | Ghidra 12.1.2, headless auto-analysis; scripts in [`tools/ghidra/`](../tools/ghidra) |
| Addresses | virtual addresses (image base 0x400000); `FUN_xxxxxxxx` are Ghidra's default function names |

`DKII-DX.exe` from the same release is a different build, so its addresses differ.

Some functions are only reached through vtables and Ghidra's auto-analysis misses them.
`tools/ghidra/Decompile.java` creates a function at the given address when there is none.

```bash
tools/ghidra/run.sh Decompile.java 0x006163c0   # GHIDRA_PROJ = an analyzed DKII.EXE project
```

The marks used below:
- **[observed]**: the behaviour follows directly from the code or data.
- **[inferred]**: a reasoned interpretation.
- **[in game]**: confirmed by playing the game.

## Overview

```
 game events ── raise(level) / set(level) ──►  music level L  (sound manager, once per game tick)
 (fights, battles,                                 │  intensity = 20 × level, then L decays to ≤ 1
  Horned Reaper, level start)                      ▼
                                       ┌───────────┴────────────┬────────────────────────┐
                                  MUSIC group               AMBIENCE group 341     ONE_SHOT_ATMOS 746
                                  (track per level)         linked player          delay-chained player
                                  linked player             intensity (+20?)       ignores intensity
                                  reads intensity at
                                  each segment boundary
```

Three background layers play at the same time. Each is a sound group (see
[file formats](file-formats.md#sfxmap-sound-groups)) played by a player class chosen from the
group's flags. The music level is the only thing the game logic controls.

## Player classes

The factory `FUN_0060da70` picks the class that plays a group from its u16 flags at `group+0x10`
**[observed]**:

| Condition | Class (vtable) | Used by |
|---|---|---|
| `flags & 4 == 0` | `FUN_006157f0` | plain sound effects |
| `flags & 0x10` | vtable `0x6741c8` (`FUN_00616630`) | options screen music (0x021e) |
| `flags & 2` and `& 0x400` | vtable **`0x674180`** (`FUN_006165e0`): linked streaming player | **music, ambience (0x0606)** |
| `flags & 2`, no `0x400` | vtable `0x674140` (`FUN_006162f0`) | not used by the background |
| neither 2 nor 0x400 | vtable **`0x6740b0`** (`FUN_00615ee0`): delay-chained player | **one-shot atmosphere (0x0204)** |
| only `0x400` | vtable `0x6740f8` (`FUN_00616090`) | not used by the background |

`FUN_00615680` is a link picker much like the music's. It belongs to the child class of
`0x6740f8` (vtable `0x673f40`), which the background does not use.

## Parameters: how the intensity reaches a player

Every playing sound instance has four parameter slots: one-byte ids at `+0x40..0x43` and values
at `+0x44..0x47`. Setting parameter `p` (`FUN_0060c750`, reached through `FUN_00608de0` →
`FUN_0060d990`) writes the value into every slot whose id is `p` **[observed]**.

The id of slot 0 comes from byte `+0x12` of the group, which is 2 for the music and the ambience.
So "set parameter 2" writes `+0x44`: that byte **is the intensity** the players read.

## The linked streaming player

This is the player for the music and the ambience (vtable `0x674180`). It plays one item at a time
on a single stream and queues the next item when the current one ends, so the playback is gapless.

### Picking the next item

`FUN_006163c0(this, intensity)` **[observed]**:

```c
if (cur == NULL && group != NULL)
    cur = &group->items[0];                       // item 1: the entry point
n = number of cur->links with min <= intensity <= max     // inclusive
if (n == 0) {
    this[+0x44] = 0x7f;                           // "stopped" marker in the intensity slot
    flags |= 0x100;
    return 0;                                     // nothing to play
}
seed = seed * 0x19660D + 0x3C6EF35F;              // global LCG at 0x7a53b0, shared with other sound code
k = rotl32(seed, 19) % n;                         // i.e. (seed >> 13 | seed << 19) % n
cur = target of the k-th matching link, counted in file order
```

- The choice is **uniform over the matching links**. No weight field is involved.
- **Repeating a link weights it.** 14 music items list a target twice in the same range. For
  example item 5 of track 3 has three links at 20-39: {6, 32, 6}, so item 6 follows with
  probability 2/3.
- An item with several sounds picks one by cumulative weight (`FUN_0060cff0`): the first sound
  with `(rotl32(seed, 19) >> 16) <= weight`. All music and ambience items have one sound.
- Volume (`FUN_0060caa0`) and probably pan (`FUN_0060cb90`) are picked in `[min, max)` of the
  item. A parameter slot can override them, but no background group uses that.

### Starting, stopping and resuming

**[observed]** unless marked:

- **Start.** Instance init `FUN_0060c9d0` calls vtable slot 15, which is `FUN_006165d0` for this
  class, i.e. the picker with `cur == NULL`. The first item that plays is therefore a random
  target of **item 1's links** at the current intensity. Item 1 is an entry point and is not played
  itself, except when it links to itself, which track 3 does.
- **Segment end.** `FUN_006164d0` (slot 0) picks the next item with the intensity in `+0x44`,
  picks the item's sound and queues it on the stream.
- **No match.** Nothing gets queued, so the layer **goes silent** after the current segment and
  stays in the stopped state: `flags & 0x100`, intensity slot `0x7f`.
- **Resume.** Slot 8, `FUN_00616560`, runs on every update. If the layer is stopped and the
  intensity slot is no longer `0x7f`, it picks from item 1's links again. The sound manager rewrites
  the intensity on every update, so in effect a stopped layer retries every update.
- Neither track's item 1 has a link below intensity 20. **At intensity 0 the music is silent**,
  and it starts as soon as the intensity reaches 20. **[in game]**: My Pet Dungeon stays silent
  until creatures fight.

The intensity is only read when an item is picked. A change takes effect at the next segment
boundary, so up to 13.2 s later for track 1 and 10.7 s for track 3. This is why the music never
cuts mid-phrase.

## The delay-chained player

This class plays the one-shot atmosphere (vtable `0x6740b0`). Each sound plays as its own voice;
the chaining is done in `FUN_00615b10` (first voice) and `FUN_00615d60` (tick) **[observed]**:

- An item is picked at random (base picker `FUN_0060d060`, weighted by the item weights at
  `item+0x1e`). The atmosphere has a single item.
- Its sound is picked by the cumulative sound weights, as above.
- The next item starts `250 ms + rand(delayMin, delayMax)` after the previous one **started**
  (`FUN_0060ccf0`, `item+0x10/+0x12`). For the atmosphere that is 8.25-20.25 s apart, while the
  sounds themselves last 0.4-4.7 s. Hence the sparse creaks and growls.
- With a delay range of 0-0 the chain ends after one item. That is why the music cannot use
  this class.

## The music level

The in-game sound manager is a static object at `0x75b880` (pointer `DAT_0075b468`, vtable
`0x66f7fc`). Its init function `FUN_00567300` looks up the `MUSIC`, `AMBIENCE` and
`ONE_SHOT_ATMOS` categories. The manager keeps the **music level** `L` at `+0x2c`
**[observed]**:

| Method | Does |
|---|---|
| `vtbl+0x4c`, `FUN_00567bc0` | **raise**: `L = max(L, x)` |
| `vtbl+0x50`, `FUN_00567be0` | **set**: `L = x` |

`L` is `0xff` after construction, and `FUN_005384f0` sets it to `0xff` again. That function
runs when a level starts **[inferred]**, which matches the silence at level start **[in game]**. The update
`FUN_005674f0` is called from the game loop (`0x526b4e`, next to the per-turn timing statistics),
so it runs once per game turn **[observed]**:

```c
intensity(L) = L == 0x100 ? 20 : L == 0x101 ? 40 : L == 0x102 ? 60 : L == 0x103 ? 80 : 0;  // 0xff -> 0

if (music option DAT_0075ad37 is on) {
    if (no level loaded || music playing)
        setParam(music, 2, intensity(L)), update(music);
    else
        music = start(MUSIC, theme_table[level byte +0x720d, 0 counts as 1]);
}
if (no ambience)  ambience = start(AMBIENCE, 341);
else              setParam(ambience, 2, intensity(L) + (DAT_0075ad27 == 1 ? 20 : 0)), update(ambience);
if (no one-shots) start(ONE_SHOT_ATMOS, 0x2ea /* 746 */);
else              update(one-shots);

if (L > 0x100)  L = 0x100;     // decay
```

What follows from this **[observed]**:

- **Levels.** `0xff` is silent; `0x100`-`0x103` are levels 1-4 with intensities 20, 40, 60 and
  80. 80 is the highest value ever used, so the 80-100 links are the top part.
- **Decay.** Every request is a `raise`, so the highest request since the last update wins. The
  decay to `0x100` after each update means a level above 1 lasts only while something keeps
  requesting it. Once the fighting ends the music falls back to level 1, part 1, and **keeps
  playing**. Only `set(0xff)` silences it again. **[in game]**: after a fight in My Pet Dungeon
  the music continues.
- **Track per level.** `theme_table` at `0x6bdb70` is `{341, 343, 344, 345}`, indexed by the
  level's sound track byte (`GameLevel.soundTrack` in OpenKeeper). Track 1 is group 343 and
  track 3 is group 345. Group 344, "track 2", does not exist in `musicSFX.map`, so a level asking
  for it would get no music. Presumably the track was cut **[inferred]**.
- **Ambience offset.** `DAT_0075ad27` is a 1/2/3 game option (its menu shows text ids 0x5a3,
  0x119 and 0x5a2). When it is 1, the ambience gets +20, so it drops out at level 1 instead of
  level 2. Which option this is remains unknown.
- **Switches.** `DAT_0075ad37` gates only the music, so the ambience and the atmosphere play even
  with the music switched off **[inferred]**: the variable sits next to the other sound
  settings, and only the music branch checks it.

## What requests which level

| Request | Where | What |
|---|---|---|
| `raise(0x100)`, level 1 | `FUN_00458370`, `FUN_004586b0`, `FUN_00482010`, `FUN_00485ab0`, `FUN_00485b80` | a creature enters creature state 0xb4, 0xb8, 0xa4 or 0xba. These are most likely the states that start a fight **[inferred]** |
| `raise(0x101..0x103)`, levels 2-4 | `FUN_00505310` | the battle threat score, see below |
| `raise(0x103)`, level 4 | `FUN_00488930` (`Creature.cpp`, per creature per tick) | a creature with type flag `0x4000` exists: the **Horned Reaper** |
| `set(0xff)`, silent | `FUN_00488930` | the same creature, when bit `0x40` at `(creature+0x2a)->+0x76` is set |
| `set(0xff)`, silent | `FUN_005384f0` | level start **[inferred]** |

**The Horned Reaper.** In `Creatures.kwd`, only the creature named "Horny" has flag `0x4000`, and
OpenKeeper names the flag `IS_HORNY`. Next to the request is a countdown at `+0x4e9` that ends
the creature, which matches the Horned Reaper's limited time after being summoned **[observed]**.
What the `0x40` bit means, the one that makes the Reaper silence the music instead, is
**unknown**.

### The battle threat score

`FUN_00504f00` (called from `0x50a815`) scores 8 battle slots, calling `FUN_00505310` for each
one. It also adds creatures that start fighting to a battle (`FUN_00504fb0`) **[observed]**:

```c
score = 0;
for each creature c in the battle (a list linked through c+0x240):
    if (c->inBattle /*+0x37a*/ && stateGroup(c) == 0x16 /*fighting*/
            && c+0x25e != 0xca && c+0x32d != 0xca && !(c->flags /*+0x3e6*/ & 0x8000)
            && c->owner /*+0x28*/ != battle->player->id) {
        threat = c->type->threat;                                   // u16 at type+0x698
        if (c->level /*+0x403*/ != 1)
            threat = threat * levelPercent[c->level] / 100;         // s16 table at (*0x6e4154)+0x105
        score += threat;
    } else if (c->battleTimeout /*+0x37e*/ <= now) {
        remove c from the battle;                                   // FUN_00505500
    }
if      (score >= MUSIC_LEVEL_FOUR_THREAT_THRESHOLD)   raise(0x103);   // 1500
else if (score >= MUSIC_LEVEL_THREE_THREAT_THRESHOLD)  raise(0x102);   // 500
else if (score >= MUSIC_LEVEL_TWO_THREAT_THRESHOLD)    raise(0x101);   // 100
```

In words: the score adds up the **threat of the creatures that are actively fighting** in a
battle against the battle's player, each scaled by its experience level. Creatures owned by that
player don't count. Whether the battle's player is always the local keeper is not established.

- **Thresholds.** These are global variables 215, 216 and 217 in `Data/editor/GlobalVariables.kwd`,
  stored as records `{u32 id, u32 value, u32 0, u32 0}`. The game's variable switch at `0x51cd90`
  stores variable `id` from case `id - 3` and puts these three into `DAT_006f29b5`, `b9` and `bd`.
  OpenKeeper's variable table names them `MUSIC_LEVEL_TWO/THREE/FOUR_THREAT_THRESHOLD`
  **[observed]**.
- **The threat value.** `type+0x698` is the creature type's **Threat** attribute **[inferred,
  strong]**. `type+0x692` is by far the most used field of that struct, 137 accesses, which makes
  it the HP. In the `Creatures.kwd` record, HP is followed by hpFromChicken, fear and threat, which
  puts threat three fields later, at `+0x698`. (The in-memory struct is not a byte copy of the
  record, so this can't be read off the file directly.)
- **The level scaling.** `levelPercent` is most likely the per-experience-level THREAT percentage
  of the creature statistics **[inferred]**. OpenKeeper computes its `Threat` component the same
  way, as `threat × stats[level][THREAT] / 100`.

## Reimplementations

- [`player/`](../player): the standalone Qt 6 player. The data, sequencing and level logic live
  in `src/core`; `Engine` runs the tick, the raise/decay and the threat-to-level mapping. Its UI
  stands in for the game with a single threat slider: 0 means no fight, other values act like an
  ongoing battle with that score.
- OpenKeeper, branch `feature/dynamic-music`:
  - `SoundSequencer`, `SequencedSoundStream` and `MusicLevel` play the three layers.
  - `MusicLevelState` requests levels from fights. OpenKeeper has no battle groups, so it counts
    the fighting enemies that are attacking the player's creatures or are attacked by them.

What is still unknown or only inferred is collected in [open questions](open-questions.md).
