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

The matrix runs in the processor and continues when the editor is closed.
Interactive chess, the simple bot, and archive replay run in the editor;
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
MIDI release, voice-pool stress, and allocation-free core rendering.
`CheckmatePluginTests` checks all five archive move lists, generated MIDI,
transport and seeks, saved-state round trips, MIDI export, variable block sizes,
and editor rendering. Linux GUI tests require an X server (for example Xvfb).

See `VALIDATION.md` for the actual checks completed with this delivery and the
remaining Windows/FL Studio acceptance steps.

## Dependencies

JUCE is fetched separately and retains its own license. Product identity is
Checkmate / Skell Audio, with a distinct VST3 plugin code of CkMt. The archive
contains source, build configuration, tests, and documentation.
