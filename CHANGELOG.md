## 2.3.0 — Voice continuity, diagnostic CPU metrics, six-page polish

- Voice stealing prioritizes released / quiet voices, protects scheduled notes, and adds an allocation-free four-millisecond continuity fade during replacement. Burst polyphony regression tests added.
- Added host-callback DSP average/recent-peak % of the real-time block budget, active voice count, cumulative steals and potential over-budget block counter. This is real-time telemetry, not an FL Studio-wide CPU statistic.
- Unified the six-page accent treatment, active-tab underline, control tooltips, clearer Game Lab row selection, and improved sample and Sound Lab panel borders.
- Fixed project-side MSVC narrowing/shadowing warnings, replaced deprecated JUCE WAV writer test API, and made float buffer initialization explicit.
- Added an optional offline CPU benchmark (`CHECKMATE_BUILD_CPU_BENCH=ON`) and a repeatable FL Studio profiling protocol. No FL Studio session is claimed to have been executed remotely.
- Keeps existing parameter IDs, sample file references, and saved-session structure compatible with 2.2.

## 2.2.0 — DSP and UI refinement

- Replaced per-sample triangle additive synthesis with precomputed, harmonic-limited tables. Tables are generated in `prepareToPlay`, never in the real-time render loop.
- Smoothed resonance automation and removed DC at a bass-safe 10 Hz after the output stage.
- Added a five-millisecond tail fade for one-shot samples to prevent end-of-buffer clicks.
- Added a vector-rendered knob with a restrained lime value ring, improved control tips and a grouped Sound Lab layout.
- Repositioned the output meter into the header so it does not overlap FX toggles.
- Extended standalone DSP tests for one-shot DC suppression, polyphony and allocation-free triangle playback.
- Parameter IDs, session serialization, preset format and original six pages remain compatible with 2.1.

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
