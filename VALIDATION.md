# Checkmate port validation — 2026-10-25

## Completed

Native Release build using GCC 13.3, CMake 4.4, and the template's JUCE 8.0.12.
Both VST3 and Standalone targets built successfully on Linux. The source ZIP
contains no build directories, downloaded dependencies, or compiled binaries.

Both CTest suites passed:

| Check | Result |
| --- | --- |
| Chess initial-position legal move counts | 20, 400, 8,902, 197,281 through depths 1–4 |
| Castling, lost castling rights, castling while in check | Pass |
| En passant, promotion to queen, Fool's Mate | Pass |
| All five supplied archive move lists | Every move replayed legally |
| Ten scales, twelve roots, six pieces, every square | Notes remain within the specified 24–108 range |
| Audio at 22.05, 44.1, 48, 96, 192 kHz | Finite samples, bounded output, nonzero energy |
| DSP reset and externally triggered MIDI note release | Pass |
| Voice-pool stress and allocation-free core rendering | Pass |
| Generated MIDI note-on and note-off | Pass; 180 ms gate ends at sample 8,640 at 48 kHz |
| Matrix start and stop | Pass |
| Host sync, transport seek, host stop | Pass using a test playhead |
| Matrix and parameter state round trip | Pass |
| Session edit after restore, then a second save/reload | Latest session retained |
| Standard MIDI export and reparse | Pass, 960 ticks per quarter |
| High-feedback delay tail reporting | Includes the long feedback decay |
| Blocks of 1, 64, 511, 4,096 samples | Finite output |
| All four editor tabs | Rendered at 1,080×720 and 810×540 |

The full-size rendered views are in `docs/screenshots`. Visual review corrected
label encoding, excessive numeric precision, dial text-box styling, and a matrix
reset button that previously referred to the hidden match board.

The tests exercise the native engine and processor. They do not establish
perceptual equivalence with the browser version, full chess-engine strength,
CPU performance on a Windows audio workstation, or host compatibility by
inspection alone. Porting differences are listed in README.md.

## Windows / FL Studio acceptance

These steps remain to be run on the user's Windows machine; FL Studio was not
available in the build environment.

1. Build Release VST3 and Standalone, then run CTest. Alternatively, run the
   included GitHub Actions workflow and download its Windows x64 artifact.
2. Install the complete VST3 bundle, rescan, load Checkmate as an instrument,
   and confirm MIDI input and audible output. Try 44.1, 48, and 96 kHz with
   small and large device buffers. Confirm the host reports zero added samples
   of plugin latency; envelopes intentionally shape the audible onset.
3. Open/close and resize the editor repeatedly. Confirm no wrapper crash on
   load, close, session save, or removal from the project.
4. Test Matrix with Host sync enabled at two tempos; start, stop, loop, and
   reposition the DAW. Close the editor and verify matrix audio continues.
5. Edit sounds, a matrix, and a match. Save/reopen the FL Studio project and
   verify restoration. The matrix should restore disarmed. Change the match,
   save/reopen a second time, and confirm the newest moves are retained.
6. Route generated MIDI to a second instrument. Test retriggers, transport
   stop, MIDI out toggle, and Panic for stuck notes. Export a matrix MIDI file
   and import it into the DAW. Record/bounce the audio through the DAW.
