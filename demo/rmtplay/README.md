# RMTPLAY for the Commander X16

Plays a Raster Music Tracker song (an Atari POKEY module) on the VERA PSG,
with a visualizer in the style of the XEX/SAP export of RMT: song name,
author and date, one volume bar per channel, the POKEY registers the player
computes (AUDF, AUDC, AUDCTL), song line / row / speed and play time.

It is the X16 port of RMTPLAY of
[VERA_ATARI_PBI](https://github.com/gianlucarenzi/VERA_ATARI_PBI)
(`vera-tests/rmt`), which runs on an Atari XL/XE with a VERA card on the
PBI: here only the VERA screen and the VERA PSG are left.

```sh
make                        # the three demo songs: build/GEMX.PRG,
                            # build/MONO.PRG, build/STEREO.PRG
make run-gemx               # x16emu -scale 2 -prg build/GEMX.PRG -run
make run-mono               #   (run-stereo, ...)
make one SONG=path/song.rmt # any RMT4 (4 channels) or RMT8 (8) module:
                            # build/SONG.PRG (PRGNAME=x: build/X.PRG)
make one SONG=path/song.sap # a SAP / XEX exported by RITMO or RMT 1.31+
make one SONG=x.sap SUBSONG=1   # another subtune of it
make one SONG=... NAME="..." AUTHOR="..." DATE="..."   # instead of the text in the file
make MHZ=4                  # the CPU clock the player timer is set for (8)
make run PRGNAME=x [SONG=...]   # build/X.PRG in x16emu
```

The demo songs (`music/`):

| PRG | song | |
|---|---|---|
| `GEMX.PRG` | `gemx.rmt` | "gem'x song", Atari version by Raster / C.P.U., 2004: RMT4, instrument speed 1 |
| `MONO.PRG` | `mono_reference_song.rmt` | the mono reference song of RMT / RITMO: RMT4, instrument speed 4 (the player 4 times a frame) |
| `STEREO.PRG` | `stereo_reference_song.rmt` | the stereo reference song: RMT8 (two POKEYs, 8 PSG voices), instrument speed 4; it starts in stereo (channels 1-4 left, 5-8 right) |

The player is built for the track count of the song (4 or 8), so each song
is a PRG of its own.

Keys: SPACE pause / play, R restart, S stereo on/off, Q (ESC, RUN/STOP)
exit.  Needs cc65 and python3.

## How it works

- `src/rmtplayr.s` is the Atari RMT 1.20090108 player routine of Raster /
  C.P.U. ported to ca65 (from AT2019 RmtSkeleton, the same source on the
  Atari, the C64 and the X16 builds).  Built with `-D RMT_SHADOW` it writes
  its POKEY registers to RAM (`pokey_shadow`), with `-D RMT_VERA` it
  exports the channel state; `-D RMT_TRACKS=8` for RMT8 (stereo) modules.
  For a `.sap` / `.xex` song `src/lzsplayr.s` takes its place: those files
  hold the POKEY registers RMT recorded frame by frame (LZSS streams of
  VU-Player), decoded one frame per call.
- `src/psgrmt.s` turns the channel state into VERA PSG voices once per
  frame (4 voices for RMT4, 8 for RMT8; 8-15 stay free):

  | POKEY | VERA PSG |
  |---|---|
  | frequency from AUDF + AUDCTL (64/15 kHz, 1.79 MHz, 16 bit pairs) | word = K / n, n = divider in machine cycles (64 kHz 8 bit from a table, `tools/mkpsgtab.py`) |
  | pure tone | pulse 50% |
  | distortion C (poly4), poly5 tones | pulse 25% |
  | poly17 / poly5 noise | VERA noise |
  | linear volume 0-15 | logarithmic VERA level of the same amplitude |

  Not reproduced: volume only mode (samples) and the high pass filters.
  The pitch is the one of the Atari the song was written on (PAL POKEY
  clock by default).
- `src/rmtx16.s` runs the player.  An RMT song is written for the 50 Hz
  VBI of a PAL Atari (60 Hz NTSC); the X16 frame is the VERA VSYNC at
  59.52 Hz, and playing a 50 Hz song from it (one VSYNC in six skipped)
  gives a jolt of 17 ms every 6 frames, well heard in songs that change
  their registers every frame.  So the player runs from Timer 1 of VIA1,
  free running at 300 Hz: a song frame every 6 ticks at 50 Hz, every 5 at
  60 Hz.  The handler is on the KERNAL IRQ vector ($0314): a timer IRQ ends
  there, a VSYNC goes on to the KERNAL (keyboard, clock).  The KERNAL uses
  Timer 1 only as an entropy source, the serial bus uses Timer 2.
- `src/rmtplay.c` draws the screen: layer 1 in 40x30 text with the X16
  upper / lower case character set and the bar glyphs, map at $00000 (the
  KERNAL screen at $1B000 is left alone and comes back on exit).  Only the
  cells that change are written; psgrmt.s saves and restores the VERA port
  registers, so the IRQ can come in the middle of a screen write.

The songs come with Raster Music Tracker / RITMO (`rmt/songs/rmt128` of
RITMO: `others/mono_reference_song.rmt`, `others/stereo_reference_song.rmt`).
