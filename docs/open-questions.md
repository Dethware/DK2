# Open questions

These are the parts of the DK II background sound that are still unknown or only inferred.
Addresses refer to the analysed `DKII.EXE` (see [runtime behaviour](runtime-behaviour.md#sources-and-how-to-check-them)).

## Game logic

- **Fight states.** Entering creature states 0xa4, 0xb4, 0xb8 or 0xba requests music level 1
  (`FUN_00482010`, `FUN_00458370`, `FUN_004586b0`, `FUN_00485ab0`/`FUN_00485b80`). They are very
  likely the "start fighting" states, but which state is which is not mapped. Do creatures of
  other keepers trigger it too?
- **Battles.** Which battles does `FUN_00504f00` score? Each keeper has battle slots. If the AI
  keepers' battles also raise the level, fights between two AI keepers would change your music.
  The condition `c->owner != battle->player` suggests the slots are per keeper, but whose slots
  reach the sound manager is not established. Also unknown: when creatures join a battle
  (`FUN_00504fb0`) and how far apart two fights can be and still count as one battle.
- **The threat value and its scaling.** `type+0x698` is inferred to be the creature's Threat
  attribute, and the table at `(*0x6e4154)+0x105` the per-level THREAT percentages. Checking
  this in a debugger would settle both.
- **The Horned Reaper silence.** While a Horned Reaper exists the level is forced to 4, unless
  bit `0x40` at `(creature+0x2a)->+0x76` is set, in which case the music is silenced instead
  (`FUN_00488930`). What does that bit mean? Possession, or the Reaper being in hand, are guesses.
- **The ambience option.** The ambience gets +20 intensity when the 1/2/3 option `DAT_0075ad27`
  is 1. Its menu shows text ids 0x5a3, 0x119 and 0x5a2; looking those up in the text tables would
  name it.
- **Level variable overrides.** The music thresholds are global variables 215-217. Can a level
  override them, as levels can override other variables?
- **Tick rate.** The sound manager updates once per game turn (called from `0x526b4e`). The exact
  rate isn't measured, but it only matters for how long a request lasts.

## Data fields

- Group `+0x0c` (6000 music, 1000 options music, 149 atmosphere): probably a priority.
- Item `+0x14/+0x15` (always 50, 50).
- Item `+0x22`, a float: 20.0, 100.0, or 0.1 for the last three items of track 3.
- Item `+0x0e/+0x0f`: picked like the volume and very likely the pan, but not seen in use.
- Entry fields `+0x04..0x0b`.
- Track 2: the theme table maps sound track 2 to group 344, which is missing from the data. Was it
  cut before release, or did it ship in another version?

## Not covered yet

- The options screen music (group 838, player class `0x6741c8`): a fixed loop, but its class
  isn't analysed.
- Player classes `0x674140` and `0x6740f8`: used by other sound groups, not by the background.
- The front end (main menu) ambience and its object sounds.
- `DKII-DX.exe`: presumably the same logic at other addresses.
