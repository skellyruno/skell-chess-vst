# Checkmate 2.3: FL Studio CPU-profiling checklist

**Important:** This guide describes a procedure to run on a real Windows FL Studio installation. Automated build tests and offline benchmarks are **not** a substitute for recording results from an actual DAW session. No FL Studio numbers are claimed until someone runs these steps.

## Instruments provided by the plugin

The right-hand side of the plugin header displays `DSP avg% / recent peak% | active voices`. These values are based on the time spent in **Checkmate's `processBlock` callback**, divided by the nominal duration of that audio block. The Sound Lab footer additionally shows cumulative voice steals and callback blocks exceeding their nominal real-time budget. The latter is a potential overload indicator **not** proof of a host xrun. The peak is a rolling/decaying peak, not a session-long maximum.

These in-plugin values **exclude** FL Studio mixer plugins, other tracks, graphics, driver overhead, and any work the host does outside Checkmate's callback. For real-world CPU impact also record FL Studio's own CPU meter and (if available) its Plugin Performance Monitor measurements. Do not confuse the two.

## Reproducible test

1. Open the 64-bit Windows VST3 `Checkmate.vst3` inside FL Studio. Open the plugin and confirm all 6 pages render, resize, switch, import PGN, and restore after closing/reopening the window.
2. Set a fixed 48 kHz sample rate, 256-sample ASIO buffer, 120 BPM, and no unrelated plugins. Use the same audio interface and PC power mode for every trial.
3. Test four sustained note workloads: **8, 32, 64, and 96 active voices**. Ensure the plugin's header voice counter matches your intended workload (chess-play events can create additional harmonies).
4. Record 15 seconds with the plugin at each workload; note plugin DSP average/peak, voice count, steals, blocks over budget, FL Studio CPU, and audible clicks. Repeat three times.
5. Repeat at **128 and 512 samples**, and if practical at 44.1 kHz and 96 kHz. Compare a freshly opened instance to one reopened from an `.flp` to cover preset restoration and custom-sample references.
6. Trigger **bursts exceeding 96 voices** and capture audio. Listen around each voice replacement for clicks at both 120 BPM and rapid ratchets. Try the original 2.2 build in an identical FL Studio project as the baseline.
7. Test transport stop/start, host sync on/off, pattern export, MIDI routing, custom sample reload, and plugin window close/reopen while playback continues.
8. Preserve a zip of the test FLP, exported recordings, and an anonymized CSV of measurements. Do not include commercial samples or licensed packs when sharing.

Suggested recording template: [FL_STUDIO_CPU_TEMPLATE.csv](FL_STUDIO_CPU_TEMPLATE.csv)

## Release gates

- No build warnings from **project source** on the pinned Windows JUCE toolchain. Warnings from upstream dependencies should be logged separately, not silently suppressed.
- No clipping, NaN/Inf, crashes, unexpected voice drops, or stuck MIDI notes across the test matrix.
- DSP time should remain safely below the callback deadline in intended live-use settings; near-100% load is a warning.
- Compare CPU measurements against Checkmate 2.2 on the same machine; don't claim a speedup from incompatible platforms or buffer configurations.
