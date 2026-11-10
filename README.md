# Checkmate — JUCE VST3 / Standalone

Native C++ port of the supplied `checkmate-synth.zip`, built on the supplied
`skell-vst-template-main.zip` CMake structure. No browser, Node, React, WebView,
API key, or web server is required at runtime.

## Build on Windows

Install Visual Studio with Desktop development with C++, CMake 3.22+, and Git.
Open a Visual Studio developer terminal in this folder:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release --parallel 2
ctest --test-dir build -C Release --output-on-failure
```

The template's pinned JUCE 8.0.12 is fetched on the first configure. To use an
existing JUCE 7+ checkout instead:

```powershell
cmake -S . -B build -A x64 -DJUCE_PATH="C:/JUCE"
```

Output bundles:

- `build/Checkmate_artefacts/Release/VST3/Checkmate.vst3`
- `build/Checkmate_artefacts/Release/Standalone/Checkmate.exe`

Copy the **whole Checkmate.vst3 folder** into
`C:/Program Files/Common Files/VST3`, then rescan plugins in FL Studio. Load it
as an instrument. The standalone app uses JUCE's audio/MIDI settings menu.
Select an output device and MIDI input there if needed.

The included GitHub Actions workflow builds, runs the tests, and packages both
Windows outputs. It runs on push, pull request, or manual dispatch.

**2.1 polish:** Archive replay now uses a separate board and will not overwrite an
in-progress Match. Host tempo affects playback only when Host sync is enabled.
The Samples view marks missing files and caches those checks to keep redraws fast.
PGN imports now handle games without results, reject unmatched variations, and
accept common zero-based castling notation with check/checkmate suffixes.

## Checkmate 2.4 — games, samples, and refined workflow

**Six tabs:** Match, Matrix, Sound Lab, Archive, **Game Lab**, and **Samples**.
The default window is now 945×630, with a fixed 3:2 resize ratio. Dark graphite,
soft ivory/gold and restrained neon accents retain the original Checkmate identity.

**2.4 performance and reliability:** Replacement WAV/AIFF samples remain safe while an
older voice is still sounding; superseded decoded buffers can be reclaimed on
the non-audio thread instead of growing indefinitely. The Archive uses its own
STEP/REPLAY transport (not the Matrix transport), and the Samples screen caches
file status instead of probing storage during every repaint. Idle full-window
repainting is throttled without slowing live meters. The factory plugin and
sample file references use the same saved-state format as earlier versions.

**Game Lab**

1. Open **Game Lab**, choose Track A, B, or C, and press **IMPORT PGN**.
   A `.pgn` file can contain more than one game; games fill available tracks
   from the selected track onward (maximum three active tracks). The PGN main
   lines are checked against the native chess engine before being accepted.
2. Select a move in the scrollable timeline. **MIDI NOTE** is 0 for automatic
   mapping or 1–128 for MIDI notes 0–127; also set velocity and note length.
3. Enable the A/B/C tracks, press **PLAY SEQUENCER** (which also enables Game sequencer),
   and select the musical subdivision. The games advance in lockstep with the
   existing matrix clock and loop at their own lengths. Enable **Host sync** to
   follow the DAW transport, or disable it for standalone free-running playback.
4. Enable **Beat mapping** for the built-in piece-to-drum MIDI note mapping.
   With samples loaded, a mapped hit triggers the original one-shot pitch;
   otherwise it uses the synth voice for that MIDI note.
5. Press **EXPORT GAME MIDI** to export *enabled* tracks to separate MIDI tracks
   in a Type 1 standard MIDI file. Notes, velocities, durations, and the
   optional harmony events are included. Editing works without playing the game.
6. **CAPTURE MATCH** converts the currently played match to the selected track;
   **USE ARCHIVE** copies the currently selected built-in Archive game.
   A chess match can still be played and heard live in the Match tab.

**Samples:** select one of the six pieces in the Samples tab, then choose
**LOAD SAMPLE**. Decoded WAV/AIFF files of at most 12 seconds are kept in
memory and played by the existing voice engine with interpolation and pitch
transposition (source note C4). Remove a sample to restore the original
oscillator. Loaded samples are immutable once published to audio playback.

**Matrix:** control **probability**, **ratchets (1–4)**, and **swing**, and save
or recall **16 user banks**. These three groove parameters currently apply
*globally* to all occupied matrix cells, not to individual cells. Generated
matrix MIDI export includes their timing/deterministic probability decisions.

**Match bot:** Balanced (original), Aggressive, Defensive, and Experimental
heuristic playing styles, selectable beside the Black bot toggle. These are
local, reproducible chess heuristics — not cloud-based machine learning.

**Chess FX** couples drive and delay to board tension, and the original Tension
control still affects filter/delay. **Harmony** adds interval voices to
capture/check game events, choosing a brighter or darker third from the game's
tension value. Both are automatable and enabled independently.

**Local preset packs:** **SAVE PACK** and **OPEN PACK** in Game Lab write/read
`.cmate` files with synth parameters, games, edited moves, matrix banks and
sample *file paths*. Sample audio itself is **not embedded**. Keep the original
sample files available or select replacement files after moving machines.
This is offline preset sharing, **not** a connected online marketplace.

**Import limitations:** PGN variations and comments are ignored (only the
main line is imported). Custom FEN starting positions are not yet supported.
A PGN is limited to 2 MB, 32 parsed games and 4096 moves per game. In the
current UI, up to three games are assigned to live tracks at once.

**Performance and reliability:** sequence snapshots and predecoded sample
buffers are published immutably to the audio callback. Disk reads and parsing
never occur in `processBlock`. The bounded voice pool and MIDI scheduling
retain the original instrument architecture. Only pre-existing oscillators
are used when custom samples are not loaded.

## Using Checkmate

- **Match:** select a piece, then a marked legal destination to hear its voice.
  Play both sides, or enable Black bot. New Match resets the board. Flip changes
  the viewing side. Promotion automatically selects a queen.
- **Matrix:** place pieces in the 8×8 grid; choose a palette piece below the
  transport. Right-click a square, or select X, to erase. The palette currently
  places white pieces; saved black pieces use the same note mapping. Columns
  advance at 1/4, 1/8, 1/16, or 1/32-note divisions. Four patterns and Clear Matrix are included.
- **Sound Lab:** global filter, ADSR, drive, reverb, ping-pong delay, width,
  lo-fi grit, and master volume. Select a piece to edit its waveform, octave,
  filter modulation, attack, release, velocity, and pan. Double-click a dial
  to restore its default.
- **Archive:** all five move lists from the web source. Choose a game and step
  or replay it at the internal tempo. The Queen's Gambit entry is fictional.
  Entering Archive from another tab starts its selected game from the beginning.
- **Factory presets:** all six supplied parameter sets are included. Selecting
  a preset also restores unspecified parameters to defaults so results do not
  depend on the previous preset.
- **Host sync:** enable it and arm Play Matrix. The matrix then follows the
  DAW's tempo, quarter-note position, transport stops, loops, and seeks. With
  Host sync off, the matrix uses its own tempo. Use this mode in Standalone.
- **MIDI:** incoming notes play the selected voice; Channel 1–6 maps MIDI
  channels to Pawn, Knight, Bishop, Rook, Queen, and King, repeating above 6.
  MIDI note-offs release externally played voices. Generated notes leave on
  channels 1–6 when MIDI out is enabled. Configure the wrapper's output port
  and receiving instrument's input port to route them in FL Studio.
- **Export MIDI:** writes one matrix scan, including knight fifths and king
  triads, as a standard MIDI file with 960 ticks per quarter and the internal
  tempo. This is a matrix export, not a recording of incoming MIDI or a match.
- **Panic:** stops the matrix and archive replay, clears delayed triggers and
  effects, releases voices, and sends note-offs for generated MIDI.

The matrix and imported game sequencers run in the processor and continue when
 the editor is closed. Interactive chess, the local bot, and original archive replay run in the editor;
archive playback pauses when the editor closes. Parameters, matrix cells,
match move history, archive position, and editor tab are saved in plugin state.
The matrix is deliberately disarmed when restoring a project; press Play Matrix
again. No audio-device settings are embedded in plugin state.

## Porting choices

This is a native recreation, not a bit-identical Web Audio render. The ten scale
maps, square-to-pitch mapping, six piece defaults, presets, and archive moves
come from the supplied app. Oscillators, envelopes, resonant filter, saturation,
quantization, delay, and reverb are native DSP. Reverb uses damped feedback delay
networks instead of the web app's random convolution impulse. A bounded output
soft clipper replaces the browser compressor. Knight voices retain a delayed
fifth; kings generate a triad. Check/capture accents use the native voice and
filter system rather than duplicating all of the browser's special cue sounds.
The default editor keeps the app's dark/gold visual identity and uses scalable
vector chess pieces.

The web recorder/download modal is replaced by host audio capture and native
matrix MIDI export. Record or bounce the plugin's audio in the DAW. The browser
piano strip and oscilloscope are replaced by host MIDI input and an output meter.
There is no bot difficulty selector: the native bot uses a small deterministic
material/capture heuristic, not a full chess search engine. Automatic draw claims
by repetition or the fifty-move rule and underpromotion are not implemented.

Chess rules now track castling rights and en passant; legal moves may not expose
the king, capture the opposing king, or castle through check. The original web
version's simplified rights/attack handling is not reused.

## Tests

The dependency-free core can be built without JUCE or a GUI:

```sh
cmake -S . -B build-core -DCHECKMATE_CORE_ONLY=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-core --config Release
ctest --test-dir build-core -C Release --output-on-failure
```

`CheckmateCoreTests` checks chess perft through depth 4, castling, en passant,
mate, promotion, scale-note ranges, DSP at five sample rates, output bounds,
MIDI release, voice-pool stress, allocation-free core rendering and one-shot
sample playback. `CheckmateImportTests` checks PGN tags, multiple games,
comments, nested variations, SAN legality, captures, checkmate and malformed
files.
`CheckmatePluginTests` checks all five archive move lists, generated MIDI,
transport and seeks, saved-state round trips, matrix and edited game MIDI exports,
custom WAV decoding/sample-path restoration, variable block sizes, and rendering
all six editor screens at large/small sizes. Linux GUI tests require an X server (for example Xvfb).

See `VALIDATION.md` for the actual checks completed with this delivery and the
remaining Windows/FL Studio acceptance steps.

## Dependencies

JUCE is fetched separately and retains its own license. Product identity is
Checkmate / Skell Audio, with a distinct VST3 plugin code of CkMt. The archive
contains source, build configuration, tests, and documentation.

### 2.2 polish notes

The sound engine now precomputes its band-limited triangle harmonics during preparation to reduce per-voice real-time CPU cost. It also suppresses DC after the final saturation stage, smooths resonance changes, and fades the ends of one-shot samples. The Sound Lab uses vector dials, a segmented layout and a relocated output meter. Presets and saved session IDs are unchanged.

### CPU profiling & voice continuity (2.3)

Voice stealing now selects the lowest-impact voice and fades the previous waveform contribution over 4 ms instead of hard-cutting. Every page displays a live DSP callback load and current voice count; Sound Lab additionally shows steal and over-budget counters. These measurements are plugin-only, not host-wide CPU. For a reproducible FL Studio session use [the profiling guide](docs/FL_STUDIO_CPU_PROFILING.md).

Build the optional, **offline (not FL Studio)** C++ benchmark with `cmake -S . -B build-bench -DCHECKMATE_CORE_ONLY=ON -DCHECKMATE_BUILD_CPU_BENCH=ON` followed by `cmake --build build-bench` and `./build-bench/CheckmateCpuBench`.
