# Audio port notes

Research on how the original does it: `RodLegacyResearch/docs/audio.md` and
`RodLegacyResearch/docs/formats/music.md`.

Moved out of the research repo so the format docs describe the original and
these describe our code.

## Engine sound: why it does not sound like the Amiga

Our engine loop lives in `src/audio/engine_sound.cpp` and
`src/audio/audio_manager.cpp`. Current behaviour:

- `AudioSampleType::ENGINE_LOOP` extracted from `srs0.wave`
- source slice offset `0`, length `480` bytes
- mono callback synthesis, nearest-neighbour lookup, direct wrap
- pitch from `UpdateRPM()` (threshold table) and `UpdatePeriod()`
  (`3579545 / period`)

This is now settled against the binary; the research doc carries the proof. The
Amiga loops **`srl0.wave` bytes 472..984** (`+0x1d8`, length `0x200`) on two
channels, and writes `period + 6` to the device.

The pointer that was being guessed at, `A4-0x2f0c`, is `srl0.wave`: the loader
allocates 64,948 bytes and reads 37,748 then 27,200 bytes into it, which are
`srl0.wave` and `srl1.wave` exactly.

**We have the wrong file, not just the wrong window.** `srs0.wave` is a
different set and is not even loaded on this path. The "480" we coded to traces
to `srs.ins` instrument 9 - bank 0, offset 32, length 448, ending at 480 -
which is a scene sample.

Four mismatches, in impact order:

1. **Wrong source region (highest).** We loop `srs0.wave[0..479]`; the Amiga
   loops `srl0.wave[472..984]`. Wrong file and wrong window, enough on its own
   to make timbre and loop continuity substantially wrong.
2. **Mono vs dual-channel (high).** The Amiga engine path sets up two channels;
   we render a mono callback. Result is flatter and harsher.
3. **Loop and interpolation quality (medium).** Nearest-neighbour with an
   integer cast index and a hard wrap gives pronounced aliasing at high
   playback rates. Linear interpolation and loop-seam handling should fix most
   of it.
4. **Period offset (low to medium).** The Amiga applies `period + 6` in one
   update path; we map period directly.

Fix order:

1. Change the engine loop source to `srl0.wave` offset `0x1d8`, length `0x200`
2. Render as a dual-channel voice, or at least stereo duplication with slight
   decorrelation
3. Add linear interpolation in `EngineSound::FillAudioBuffer()`
4. Apply the `+6` period offset on the period-driven path
5. Keep the existing RPM threshold mapping, then tune by ear against a capture

Validate A/B against emulator captures: record period values and audible output
while idling and revving, then compare idle timbre, the acceleration sweep,
top-end harshness, and the drive-in/drive-out transitions.

The headline conclusion is that the RPM mapping is probably not the problem.
We are looping the wrong window of the sample and simplifying the channel
setup.

## Music: what needs calibrating

### Period table

We use ProTracker standard middle-octave values (C=428 ... B=226). The game has
its own 12 x uint16 table at `A4-0x451c`. Read 24 bytes from there in Ghidra or
vAmiga to get the real values. Until then, music plays at the wrong pitches.

### Already correct - do not "fix" these

`amiga_music.cpp` was brought in line with the original at some point and an
older TODO list here described it as still broken. Checked against the binary,
these are all right and need no work:

- `.ins` field layout (`env_index`, `unknown`, `bank_index`, `length`,
  `offset`), with length and offset treated as **byte** counts
- `bank_index` N maps to the bank with N in its filename; `wave_bank_[1]` is
  `srl1.wave`
- `kCiaClockHz = 709379`, with `irq_hz = clock / cia_timer_period_` - the CIA-A
  Timer A model, not a 50 Hz VBL
- `0x84`/`0x85`/`0x97` drive `speed_ticks_`; `0x92`/`0x93`/`0x94` drive
  `cia_timer_period_` scaled by 1000
- `kPeriodTable` is the game's real 12-entry table, not ProTracker values
- `EnvelopeTick` is implemented and runs every IRQ, with `MusicTick` gated by
  the divider - the same order the original uses
- `0x83` loop semantics, including the rearm to `-1`, match the assembly
  exactly, and `loop_counter.fill(-1)` matches the game's `0xFFFF` init
- `0x95` reloading only the non-zero channels from the current index

### Fixed - muting killed music permanently

`music_muted=1` in the config silenced **both** music sources for the entire
run, with no way to recover from inside the game.

`AssetManager::LoadSettings` applies `music_muted` before `music_source`. The
old `SetMusicSource` did:

```cpp
musicSource_ = src;
if (musicMuted_) return;   // Play() never reached
```

So on a muted config the player object was never started at all. Clicking the
garage's "Squelch it" zone later called `SetMusicMuted(false)`, which only
re-applies amplitude and volume - there was nothing running to raise the volume
of, so it appeared to do nothing. The mute was sticky across restarts and
unrecoverable.

Muting is now a gain control: `SetMusicSource` always calls `Play()` and passes
0 for the volume when muted. Unmuting then works through the existing
`SetMusicMuted` path and does not restart the track.

Watch for this shape elsewhere: any "muted" early-return that skips
*initialisation* rather than just *output* has the same failure mode.

### Fixed - three bugs in ResetState

All three were in `AmigaMusic::ResetState`, and all three are now corrected.

**1. Arrangement count was 4, should be 3 - the music got stuck.**
`arrangement_len_` was `patterns_.size()`, which is 4 for `msr.note`. The
original hardcodes the modulus to 3, written once at audio init and never
recomputed from the file, so it never plays step 3 at all.

This was not cosmetic. Step 3 is the only step whose channel 0 stream contains
no `0x95`, so nothing could ever call `AdvanceArrangement` again. Playback
reached step 3 and **stayed there permanently**, where the original loops 0-1-2
forever.

Now `min(kArrangementSteps, patterns_.size())`, clamped so a note file with
fewer steps cannot index out of bounds. Do not "simplify" this back to the file
length - the original does not derive it from the file.

**2. The start step is random.** The original picks it with
`random(arrangement_count)` at music start, so a scene opens on a random one of
the three steps; we always opened at 0. The decompile hides this: it drops the
call's argument and reads like an unbounded value assigned straight to the
index. The assembly pushes the arrangement count as the bound.

**3. Master volume is 9 during music, not 15.** The game sets `0xF` at audio
init but **9** on every music start. We held 15 throughout, so everything sat
louder against the envelope curve, which feeds
`target = cur * note_volume / 64 * master_volume_ / 15`.

## Music: not implemented

- **Sample sustain.** `FillBuffer` sets `sample_active = false` once
  `sample_pos` passes `sample_len`, so a sample plays once and stops. Paula
  given a length keeps re-reading it until the channel is retriggered or
  stopped, so anything held longer than its sample goes silent early here.
  Worth checking `MusicPlayNote` for how the original arms the channel before
  changing this - an earlier note in this file claimed we already loop from the
  beginning, and that was wrong in the other direction.
- **Per-channel mute.** The original has a mute flag per channel; our sequencer
  always plays. Not needed for basic playback.
- **Racing music.** There is no racing music. Not sure what's in `srl1.wave` / `srl.ins` / `srl.note`.
- **`0x83` loop opcode.** Settled: low nibble = slot, high nibble = **number of
  jumps taken**. Counters start at `0xFFFF` and rearm themselves to `-1` on
  completion, so a loop works again on a later pass:

  ```
  if counter <  0: counter = (param >> 4) - 1; jump
  elif counter == 0: counter = -1; advance past
  else: counter--; jump
  ```

- **`0x95` advance.** Only channel 0 increments the arrangement index; the
  other three reload from whatever it left. The increment is modulo the
  arrangement count, so music wraps forever.

## What to test first

1. Run the game and check the log for "AmigaMusic: X patterns" - that confirms
   note parsing worked.
2. Listen for anything at all on the music channel. Silence means the wave file
   did not load.
3. Wrong pitches: adjust `PERIOD_TABLE` in `amiga_music.cpp`.
4. Wrong tempo: adjust `TICK_RATE`.
