# MAME-Grounded Neo Geo Audio Reuse Plan

This note captures the audio reuse strategy discussed for `neo-recomp`: use
MAME's Neo Geo audio-device behavior as the primary reference, while keeping a
small runtime-owned audio subsystem rather than embedding all of MAME.

## Short Answer

The audio shortcut should not be "drop in all of MAME audio". The practical
shortcut is to make our `neo_audio` path MAME-shaped:

```text
recompiled 68000
    -> Neo Geo runtime sound write
        -> MAME-like sound latch and NMI edge behavior
        -> MAME-like Z80 memory and IO maps
        -> MAME-like 50 us interleave boost after sound writes
        -> Z80 M1 program
        -> ymfm-backed YM2610
        -> host audio output
```

The current project already reuses the most useful low-level chip component:
`ymfm`, the same Yamaha FM family core used by MAME. The remaining likely bugs
are in the scheduler, latch, NMI, and Z80 side, not in the basic YM2610 sample
synthesis code.

## Current Local State

Current `neo-recomp` audio pieces:

- `runtime/src/neogeo_audio.c`: Z80-side M1 bus, sound latch, banking, YM2610
  IO, diagnostics, and audio generation entry points.
- `runtime/src/neogeo_ym2610.cpp`: small YM2610 wrapper around vendored `ymfm`.
- `third_party/ymfm/`: BSD-3-Clause Yamaha FM/SSG/ADPCM core.
- `third_party/superzazu/`: current MIT Z80 core.
- `tools/sdl_live_host.c`: host-side audio scheduling, command delivery,
  preadvance, and SDL queueing.

Current MAME reference files inspected locally:

- `/Users/dudu/Projects/references/mame/src/mame/neogeo/neogeo.cpp`
- `/Users/dudu/Projects/references/mame/src/mame/neogeo/neogeo.h`
- `/Users/dudu/Projects/references/mame/src/devices/machine/gen_latch.cpp`
- `/Users/dudu/Projects/references/mame/src/devices/machine/gen_latch.h`
- `/Users/dudu/Projects/references/mame/src/devices/sound/ymopn.cpp`
- `/Users/dudu/Projects/references/mame/src/devices/sound/ymopn.h`
- `/Users/dudu/Projects/references/mame/src/devices/cpu/z80/`

MAME Neo Geo and latch sources are BSD-3-Clause. If code is copied or adapted,
keep license headers and add a vendored attribution file. The project can remain
public-domain for its own code while carrying BSD-3-Clause third-party code.

## What MAME Does For Neo Geo Audio

Important MAME behavior to mirror:

```cpp
void neogeo_base_state::audio_command_w(uint8_t data)
{
    // glitches in s1945p without the perfect_quantum here
    m_soundlatch->write(data);
    machine().scheduler().perfect_quantum(attotime::from_usec(50));
}
```

MAME audio CPU setup:

```text
Z80 clock:       NEOGEO_MASTER_CLOCK / 6  = 4 MHz
YM2610 clock:    NEOGEO_MASTER_CLOCK / 3  = 8 MHz
sound command:   generic_latch_8_device
sound reply:     generic_latch_8_device
NMI line:        input_merger_device driven by latch pending plus enable bit
YM IRQ line:     YM2610 IRQ to Z80 IRQ0
```

MAME Z80 memory map:

```text
0000-7fff  bank_audio_main, cart M1 by default
8000-bfff  audio_8000 bank
c000-dfff  audio_c000 bank
e000-efff  audio_e000 bank
f000-f7ff  audio_f000 bank
f800-ffff  Z80 work RAM
```

MAME Z80 IO map:

```text
port 00      read sound latch, write clears latch value
ports 04-07  YM2610 read/write
port 08/18   write NMI enable/disable, selected by address bit 4
ports 08-0b  read bank select, high byte chooses bank entry
port 0c      write sound reply latch
```

MAME initial bank entries:

```text
audio_f000 = 0x1e
audio_e000 = 0x0e
audio_c000 = 0x06
audio_8000 = 0x02
```

This is described in MAME as a compatibility hack for early cartridges and is
already mirrored by the current runtime.

## What We Already Reuse Correctly

Keep the current `ymfm` path unless a trace proves otherwise:

- MAME's YM2610 device also uses `ymfm`.
- We set `OPN_FIDELITY_MED`, matching MAME's `SSG_FIDELITY` choice.
- We use the Neo Geo YM2610 clock, 8 MHz.
- We use MAME-like stereo routes:
  - route 0, SSG, gain 0.84 to left and right
  - route 1, FM plus ADPCM, gain 0.98 to left
  - route 2, FM plus ADPCM, gain 0.98 to right
- We present Metal Slug's V-ROM chunks as a contiguous ADPCM-A region, with
  ADPCM-B falling back to the same region when no explicit B region exists.
- We mirror MAME's 128 KiB M1 `ROM_RELOAD` effective region behavior.

This means replacing `runtime/src/neogeo_ym2610.cpp` with MAME's whole
`ym2610_device` wrapper is probably lower value than fixing latch, Z80, and
interleave behavior.

## Likely Remaining Problem Area

The most likely audio bugs are in these areas:

1. **Z80 CPU accuracy**
   - Current core is `superzazu/z80`, not MAME's Z80 core.
   - MAME's core has specific undocumented flag, HALT, NMI, IRQ, and timing
     behavior that M1 programs may rely on.

2. **NMI line and latch pending semantics**
   - MAME uses a `generic_latch_8_device` with pending state.
   - Latch read acknowledges pending when separate acknowledge is false.
   - Latch clear writes zero to the stored value but does not necessarily model
     the same thing as pending acknowledge unless we explicitly mirror it.
   - MAME drives the Z80 NMI line through an input merger combining latch
     pending and the NMI enable bit.

3. **Scheduler interleave**
   - MAME's `perfect_quantum(50us)` does not mean "run the Z80 instantly and
     borrow cycles".
   - It temporarily forces very tight scheduler interleave for 50 us after the
     68000 sound write.
   - Our current host approximates this in `tools/sdl_live_host.c`; the model
     should move into `NgNeoAudio` and become time-based.

4. **YM timer and IRQ timing**
   - YM2610 timers assert IRQ to Z80 IRQ0.
   - The Z80 instruction boundary at which IRQ is taken can affect M1 control
     flow and write timing.

## Preferred Implementation Plan

### Phase 1: Make `neo_audio` MAME-shaped

Port or precisely reimplement these MAME device behaviors into our small audio
runtime:

- `generic_latch_8_device` pending/value behavior.
- Sound latch and reply latch state.
- Audio NMI input merger behavior.
- `audio_cpu_enable_nmi_w`, with `out $08` enabling and `out $18` disabling.
- `audio_cpu_bank_select_r`, with high address byte selecting bank entry.
- `audio_map` and `audio_io_map` as explicit comments/tests in
  `runtime/src/neogeo_audio.c`.

Expected tests:

- Reading port `$00` returns the latched command and clears pending state.
- Writing port `$00` clears the latch value to `$00`.
- A second 68000 write before Z80 read overwrites the first command, matching a
  single MAME latch, not a FIFO.
- NMI only fires when the latch is pending and NMI is enabled.
- `out $08` and `out $18` affect only NMI enable state.
- Bank select reads update the matching bank by high address byte.

### Phase 2: Move command interleave into `NgNeoAudio`

Move the MAME `perfect_quantum(50us)` approximation out of `tools/sdl_live_host.c`
and into the audio subsystem.

Proposed API shape:

```c
void ng_neogeo_audio_write_command_at_m68k_cycle(NgNeoAudio *audio,
                                                 uint8_t command,
                                                 uint64_t m68k_cycle);
void ng_neogeo_audio_advance_to_m68k_cycle(NgNeoAudio *audio,
                                           uint64_t m68k_cycle);
```

Runtime behavior:

- Maintain local 68000 time and Z80 time.
- Convert 68000 cycles to Z80 cycles using the 12 MHz to 4 MHz ratio.
- On sound write, latch command at the exact 68000 cycle.
- For the next 50 us, advance in small interleaved slices rather than one large
  Z80 catch-up block.
- Keep audio sample generation tied to elapsed emulated time, not to the number
  of command-service shortcuts.

Expected tests:

- One command write produces a temporary 50 us boosted interleave window.
- Z80 time does not double-advance after the boost window.
- Generated audio frame count follows emulated 68000 time only.

### Phase 3: Add MAME-comparable audio tracing

Add a compact trace log so we can compare our runtime against MAME or an
instrumented MAME reference.

Trace events:

```text
m68k_cycle, event, value, z80_cycle, z80_pc
sound_write command
latch_read command
latch_clear
nmi_enable
nmi_disable
nmi_assert
nmi_service
irq_assert
irq_clear
bank_select region, bank
ym_addr port, reg
ym_data port, reg, data
adpcma_keyon channel, start, end, level, pan
adpcmb_keyon start, end, delta, level, pan
reply_write value
```

Expected output targets:

- `build/audio_trace_ours.log`
- optional future `build/audio_trace_mame.log`

### Phase 4: Decide on the Z80 core

After Phase 3, compare behavior. If traces diverge before YM writes, the Z80 or
scheduler is suspect.

Preferred long-term path:

- Port MAME's BSD-3-Clause Z80 core into a standalone wrapper for `NgNeoAudio`.
- Keep it isolated under something like `third_party/mame_z80/` with license and
  vendored metadata.
- Expose only the small API needed by `neo_audio`:

```c
typedef struct NgMameZ80 NgMameZ80;
void ng_mame_z80_reset(NgMameZ80 *cpu);
void ng_mame_z80_set_nmi(NgMameZ80 *cpu, int asserted);
void ng_mame_z80_set_irq0(NgMameZ80 *cpu, int asserted);
uint32_t ng_mame_z80_run_cycles(NgMameZ80 *cpu, uint32_t cycles);
uint16_t ng_mame_z80_pc(const NgMameZ80 *cpu);
uint64_t ng_mame_z80_total_cycles(const NgMameZ80 *cpu);
```

Fallback path:

- Keep `superzazu/z80` if the trace shows it matches Metal Slug's M1 behavior
  through command handling, bank switching, YM writes, ADPCM key-ons, and IRQs.

## What Not To Do First

Avoid these until there is trace evidence:

- Do not embed all of MAME as a library just for sound.
- Do not replace `ymfm` with MAME's device wrapper while keeping our current
  scheduler approximation.
- Do not tune arbitrary audio constants without first tracing latch, NMI, Z80,
  YM write, and IRQ timing.
- Do not treat the runtime sound command queue as the hardware latch. The queue
  is a host/runtime event transport. The Z80-visible device is a single pending
  byte, matching MAME's generic latch.

## First Concrete Patch Slice

The best next patch is small and testable:

1. Add an explicit MAME-style `NgNeoAudioLatch` helper.
2. Route command reads and clears through that helper.
3. Route reply writes through a second latch helper.
4. Add tests named around MAME latch semantics.
5. Keep current `ymfm` output untouched.
6. Keep current host scheduling untouched until the latch behavior is green.

After that, move the 50 us command-interleave behavior into `NgNeoAudio` and
make the SDL host call the new time-based API.

## Investigation Findings (2026-06-29)

A measurement pass was run against the live host and a coupled audio-probe
capture path (WAV dump plus a per-write YM register trace, both env-gated; see
`NG_MSLUG_SDL_AUDIO_WAV`, `NG_MSLUG_SDL_YM_TRACE`, `NG_MSLUG_SDL_AUDIO_DIAG`,
and `NG_PROBE_WAV`/`NG_PROBE_SECONDS`). The headline conclusion is that the
audio subsystem timing is correct, contrary to the earlier "sped up" note.

Measured facts:

- **Sample generation rate is exact.** 300 emulated frames produced 243302
  output frames = 811.0 frames/emulated-frame = 48000 Hz / 59.185 Hz. Audio
  duration tracks emulated time 1:1.
- **YM2610 Timer A fires at the correct rate.** With Metal Slug's programmed
  `NA = 691` (TA regs `$24=$AC`, `$25=$03`), the expected period is
  `(1024-691) * 144 = 47952` master clocks. Instrumenting the actual timer
  callbacks measured ~47986 master clocks/fire (<0.1% error), i.e. 166.8 Hz.
  The driver issues two `$27=$35` re-arms per tick, which earlier made the tick
  look like ~334 Hz; counting real fires disproves any 2x speedup.
- **Native YM stream rate is correct** at clock/144 = 55555 Hz (MED fidelity).
- **FM note pitches are set from sensible musical f-num/block values**
  (a high detuned-unison fanfare on FM ch1/ch2, blocks 6-7, ~660-1050 Hz).

Observed actual defect:

- The BIOS jingle renders as a coherent ~5 s one-shot melody at a steady tempo
  and then ends. After it ends the M1 keeps running its tick loop at 166.8 Hz
  but emits only `$27` re-arms (no notes), i.e. it has no further track to play.
- Over 84 s of attract/gameplay the 68000 issued only ~16-31 sound commands and
  then looped the same three (`$0C $10 $DF`). ADPCM-A/B never key on
  (`keyons=0`).

## MAME Reference Comparison (2026-06-29)

A full MAME comparison was then run (Phase 3, executed). The `mslug` romset was
reconstructed directly from the NeoSD `.neo` (all of P/S/M/V1/V2/C1-C4 hash to
MAME's exact CRCs; P is the two 1 MiB halves swapped, C is the standard 16-bit
deinterleave), and `neogeo.zip` was built from the local BIOS files. MAME runs
it headless (`mame mslug -rompath ... -video none -seconds_to_run N`). Reference
captures used `-wavwrite` plus Lua taps on the 68000 sound-command write
(`0x320000`) and the audio-CPU YM IO ports (`$04-$07`).

Results that change the conclusion above:

- **MAME sustains the music; we do not.** MAME boots silently ~4.3 s then plays
  continuous BGM; our run plays a ~4-5 s burst then goes silent. So there *is* a
  real defect.
- **It is not the 68000 command stream.** MAME's first commands match ours
  (`$01 $03 $03 $02`). Feeding our M1 that exact sequence in isolation (probe,
  no further commands) still dies at ~4.5 s, so the fault is on the M1 side.
- **It is our M1 Z80 execution.** Same `201-m1.m1`, same commands, but the YM
  register-write streams diverge. Aligned on the music-init anchor
  (`p1 21 00`), they are identical for 33 writes, then ours **skips a block of
  operator TL writes** (`p1 41/49/45/4d = 7f`, FM ch2/ch3) that MAME performs.
- **IRQ delivery is fine.** The M1 runs IM1, the YM Timer A IRQ asserts and is
  taken (~185/s). But our M1 also rewrites timer-control `$27` ~334x/s where
  MAME writes it ~1-2x/s, i.e. ours falls into a degenerate path after the
  control-flow divergence.

## Root Cause and Fix (2026-06-29)

Tracing the M1 IRQ path pinned it precisely. The M1 sound driver re-armed Timer
control `$27` ~2x per timer fire (~334/s) where MAME does ~1x, and its IM1
handler ran **twice** per Timer-A period (two `$27=$35` writes ~483 Z80 cycles
apart, then a full-period gap).

Root cause: the **YM2610 IRQ -> Z80 IRQ0 line is level-sensitive**, but the
runtime drove it as a one-way latch. `ng_audio_step_z80()` asserted the Z80 INT
whenever the YM IRQ was pending, but never withdrew it. superzazu latches
`int_pending` until the interrupt is taken. So while the M1's handler ran with
interrupts disabled (before it cleared the timer flag), we kept re-latching
`int_pending`; after the handler cleared the flag and `RETI`'d, that stale latch
fired a **spurious second interrupt**, double-running the music tick. The
sequencer therefore advanced at ~2x -> the jingle was audibly sped up and the
track raced to its end and stalled.

Fix (`runtime/src/neogeo_audio.c`): when the YM IRQ is no longer pending,
deassert the Z80 INT by clearing `cpu.int_pending`, mirroring MAME's
level-sensitive line. After the fix the handler runs once per Timer-A fire
(`$27` ~166/s), the music plays at the correct tempo, and the audible duration
of the same sequence roughly doubles (no longer 2x fast). Regression guard:
`test_audio_timer_irq_is_level_sensitive` (counts handler services per timer
period; fails on the doubled count without the fix).

Remaining (separate, non-audio): in headless attract the 68000 only issues a
handful of sound commands then loops, so sustained attract/BGM still depends on
68000 game-progression completeness, not the audio subsystem.
