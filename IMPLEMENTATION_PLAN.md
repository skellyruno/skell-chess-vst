# Checkmate 2.0 — architecture, milestones and regression gates

## Invariants
- Existing instrument + VST3 IDs, 26 global parameters and all 42 piece parameters retain their identifiers and defaults.
- Chess legality, existing presets, matrix voices, archive and VST MIDI output stay working.
- Audio callback does not touch disk, parse PGN, take locks, or allocate sample buffers.
- Old sessions without new state nodes restore safely; audio transport starts stopped.
- No simulated store or cloud AI dependency. Offline playable bot styles + shareable local packs only.

## Data model
1. Header-only JUCE-free `GameSequence.h` parses one or more PGN main lines and validates every SAN move against `Chess`.
2. Each `MusicalMove` records the legal move, destination, signed piece, cue type, edited MIDI note/velocity/gate, and SAN label. Limits prevent unbounded input.
3. GUI creates game sequences and sample data on its own thread. A new immutable sequence snapshot is atomically published to the processor; previous snapshots remain owned until destruction, avoiding callback locking and use-after-free.
4. Stereo sample data is validated and preloaded in RAM. A processor-published bank is immutable. Synth voices reference the immutable buffers and render using interpolation/pitch stepping; the synth retains oscillator fallback.
5. Host position, free-running step counter and matrix transport drive both matrix and enabled game tracks. Transport handling remains sample-accurate.
6. State stores editable sequences and sample paths; no bundled WAVs, executable code or network connections.

## UI
- Retain Match / Matrix / Sound Lab / Archive; add Game Lab and Samples tabs.
- Game Lab: import PGN, choose track A/B/C, edit move note/velocity/length, enable playback, export MIDI, copy current match, load factory archive.
- Samples: select each of six piece voices, browse/remove sample, preview via board and show load status.
- Modern restrained dark steel/ivory/gold with neon accents, smaller resize-safe default, context-specific controls.
- Matrix: probability, ratchets, swing, 16 user banks. Match: bot playing styles. Sound Lab: chess-controlled FX/harmony toggles.
- Share packs locally via file chooser; never claim an online marketplace exists.

## Regression gates
- Core CTest: imported multi-game PGN, comments/variations, castling/capture/promotion, illegal SAN; sample render safety, current chess perft, DSP allocations.
- JUCE plugin tests: state round trip, generated MIDI export including edits, audio samples, step sequencer, UI small and large sizes.
- GitHub Windows VST3/Standalone CI remains the authoritative platform build. Linux cannot substitute for FL Studio in-host verification.
