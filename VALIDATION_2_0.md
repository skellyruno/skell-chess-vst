# Checkmate 2.0 validation ledger

## Completed here (Linux, dependency-free)
- CMake `CHECKMATE_CORE_ONLY=ON` configure and build succeeded.
- `ctest`: **CheckmateCore** passed (existing chess perft, capture rules, DSP stress, sample rendering, no per-sample allocations).
- `ctest`: **CheckmateImport** passed (multi-game PGNs, tags, comments/variations, legal SAN, capture/mate, bad input, move numbering).
- All core tests passed again with `-Wall -Wextra -Wpedantic -Werror`.
- All core tests passed with AddressSanitizer and UndefinedBehaviorSanitizer enabled.
- New engine source and six-page UI have been integrated into the ZIP.

## Windows CI pipeline (source included, not executed here)
- `.github/workflows/build.yml` uses Visual Studio Windows runners to fetch JUCE 8.0.12,
  compile VST3 and Standalone, run `CheckmatePluginTests`, and package the binaries.
- `CheckmatePluginTests` now covers edited game MIDI, playback, state round-trip,
  custom WAV loading and six-page UI renders, in addition to prior tests.

## Not verified in this environment
- Full JUCE build: the build runner here has no installed JUCE checkout and cannot
  fetch its sources through the current network environment.
- Native Windows VST3 wrapper acceptance in FL Studio, latency and release/close tests.
- Visual inspection of newly rendered GUI screenshots from a compiled JUCE editor.
- Drag/drop MIDI between plugin and DAW (use the working MIDI Export file flow).

## Suggested Windows release gate
1. `cmake -S . -B build -A x64 -DBUILD_TESTING=ON`
2. `cmake --build build --config Release --parallel 2`
3. `ctest --test-dir build -C Release --output-on-failure`
4. Open the Standalone app; test every tab, load WAV, play a PGN, export MIDI.
5. Insert VST3 in FL Studio; test tempo sync, MIDI route, project reopen with
   files present/missing, sample replacement, normal plugin close and panic.
6. Verify the screenshots and that all controls fit at 810×540 and 945×630.

**Do not label this package a tested Windows VST3 build until these gates pass.**
