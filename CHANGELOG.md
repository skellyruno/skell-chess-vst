# Checkmate release notes

## 2.1.0 — reliability and UI polish

- Archive playback has its own board and preserves the playable Match across tab changes, resets, preset state and archive stepping.
- Separate Archive progress can be restored from saved sessions.
- Free-running BPM now honors the Checkmate parameter even when the host supplies its own tempo; Host Sync continues following DAW BPM.
- The PGN importer splits new header blocks even when result tokens are absent, supports `0-0`/`0-0-0` notation with annotations, rejects stray variation closes and enforces a 32-game maximum.
- Game Lab's play control now identifies sequencer playback and enables the Game sequencer toggle when started there.
- Sample-file chooser targets the selected piece at launch, even if selection changes before the file dialog returns.
- The Samples page highlights missing external files with a timed status cache instead of filesystem reads every redraw.
- Added native engine tests for PGN edge cases, host tempo, archive navigation, state retention and UI rendering.
- Updated CMake plugin version metadata to 2.1.0 while retaining the same plugin identifiers.

### Verification
- Core and PGN tests: passed locally with strict warnings and AddressSanitizer/UndefinedBehaviorSanitizer.
- Windows VST3 and Standalone builds: validated by GitHub Actions workflow run for this release only after CI completion.
- FL Studio manual host testing: still required.
