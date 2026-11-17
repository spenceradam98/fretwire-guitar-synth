# Fretwire (working name)

Guitar synthesiser plugin: guitar in (mono or 6-channel hex), pitch tracking, 16-voice synth,
FX, 316 presets, 6 themes. JUCE 8.0.9, VST3 + Standalone. See `DESIGN.md`.

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release        # fetches JUCE if ./JUCE is absent
    cmake --build build --config Release

QA tool: add `-DFRETWIRE_BUILD_TOOLS=ON`, then `FretwireRender track | render <dir> [--wav] | calibrate <levels.h> | snap <dir>`.
Edit presets in `tools/gen_presets.py`, run it, then re-run `calibrate`.
