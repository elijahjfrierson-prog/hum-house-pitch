# HumHouse Pitch Shifter

A granular pitch shifter for guitar, in the spirit of Logic Pro's Pitch Shifter,
plus a dual-tracking doubler so one take sounds like two. Ships as **VST3**,
**Audio Unit** (macOS) and a **Standalone** app, with one-click installers that
present the licence before installing.

This is an original, clean-room implementation. It is not affiliated with or
derived from Apple's plug-in — only the workflow is familiar.

## What it does

**Pitch** — shift by semitones (−24…+24) and cents (−100…+100), with a wet/dry
Mix. Cents alone gives you the detuned doubling that thickens a rhythm part;
semitones give octaves, fifths and fourths for harmony parts.

**Timing** — Grain Size (3…100 ms) with an Auto mode that scales the grain to the
interval, and Crossfade to control how the grains splice. Short grains are tight
and grainy, long grains are smoother but smear transients.

**Dual Tracking** — the doubler. Detune (0…50 ¢), Delay (0…80 ms), Width and
Level place a second, slightly-late, slightly-detuned voice against the original.
Both voices run continuously and only their gain moves, so switching Dual Track
on mid-take does not click.

**Output** — Low Cut on the wet path (20…800 Hz), useful for keeping an
octave-down part out of the bass, plus output Gain and a Mono Input switch for
DI'd guitars.

**Presets** — Guitar Double Track, Wide 12-String, Octave Down Riff, Octave Up
Shimmer, 5th/4th Harmony, Detune Chorus, Fat Rhythm Stack, Bass Octaver, Grain
Warble.

## Install

Grab the installer for your platform from the
[latest release](https://github.com/elijahjfrierson-prog/hum-house-pitch/releases),
or from the artifacts of a CI run.

- **Windows** — run `HumHouse-Pitch-Shifter-Windows-Setup.exe`. Accept the
  licence and it installs the VST3 into `C:\Program Files\Common Files\VST3\` and
  the Standalone into Program Files. Rescan plug-ins in your DAW afterwards.
- **macOS** — open `HumHouse-Pitch-Shifter-macOS.pkg`. Accept the licence and it
  installs the VST3 to `/Library/Audio/Plug-Ins/VST3/` and the Audio Unit to
  `/Library/Audio/Plug-Ins/Components/` (Logic Pro).
- **Linux** — copy the `HumHouse Pitch Shifter.vst3` folder into `~/.vst3/`, or
  run the Standalone binary.

## Guitar quick-start

1. Insert it on the guitar track, after the amp/cab.
2. **Guitar Double Track** — Mono Input on, Dual Track on, ~14 ¢ detune, 24 ms
   delay, full width. This is the doubler: one take, two guitars.
3. **Octave Down Riff** — Mix around 55 % and the wet Low Cut near 45 Hz so the
   octave sits under the riff instead of fighting the bass.
4. **5th Harmony** — pan the wet side away from the dry for a harmonised lead.

Small detunes are the smoothest setting; wide intervals are deliberately grainy,
which is exactly the character this style of shifter is known for.

## Build from source

Requires CMake 3.22+ and a C++17 compiler. JUCE 8.0.4 is fetched automatically.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel 2
ctest --test-dir build -C Release --output-on-failure
```

Linux also needs the usual JUCE dev packages (ALSA, X11, FreeType, GL) — see the
`Install Linux dependencies` step in
[.github/workflows/build.yml](.github/workflows/build.yml).

Build outputs land in `build/HumHousePitch_artefacts/Release/`.

### Packaging

```bash
# macOS: guided .pkg with the EULA pane
./scripts/package_macos_pkg.sh

# Windows (from the repo root, after a Release build)
"C:\Program Files (x86)\Inno Setup 6\ISCC.exe" installer\windows\HumHousePitchShifter.iss
```

## Layout

| Path | What's in it |
| --- | --- |
| `SourcePS/PitchShiftEngine.*` | The DSP: delay-line granular shifter, doubler, wet low cut. No JUCE dependency. |
| `SourcePS/PitchProcessor.*` | Plug-in, parameters, presets, host state. |
| `SourcePS/PitchEditor.*`, `PitchLookAndFeel.h` | The UI. |
| `tests/EngineTests.cpp` | DSP guarantees: pitch accuracy, level, unity passthrough, doubler decorrelation, click-free sweeps. |
| `tests/ProcessorTests.cpp` | Plug-in guarantees: parameters, preset recall, state round-trip, odd block sizes, mono. |
| `installer/`, `scripts/` | EULA and installers. |

## Licence

See [installer/LICENSE.txt](installer/LICENSE.txt) — the same EULA the installers
present. Built with [JUCE](https://juce.com); VST is a trademark of Steinberg
Media Technologies GmbH.
