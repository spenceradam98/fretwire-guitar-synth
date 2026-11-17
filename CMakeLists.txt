cmake_minimum_required(VERSION 3.22)

# macOS: universal binary (Intel + Apple Silicon), minimum macOS 11 (Big Sur).
# No CPU-specific flags anywhere (SSE2 baseline, no AVX/AVX2) so 2015-era Intel Macs are fine.
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS architectures")
# Windows: static MSVC runtime so users don't need the Visual C++ redistributable.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
project(Fretwire VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE: use a local checkout if present, otherwise fetch 8.0.9 (same as the other plugins)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt)
    add_subdirectory(JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.9 GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

set(FRETWIRE_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/SynthEngine.cpp
    Source/Wavetables.cpp
    Source/FactoryPresets.cpp
    Source/PresetManager.cpp)

# A guitar synth is an insert effect on a guitar track that also accepts MIDI,
# so it is built as an effect ("Fx|Instrument") with a MIDI input.
juce_add_plugin(Fretwire
    COMPANY_NAME "Fretwire Audio"
    BUNDLE_ID com.fretwireaudio.fretwire
    PLUGIN_MANUFACTURER_CODE Frtw
    PLUGIN_CODE Frw1
    FORMATS VST3 Standalone
    PRODUCT_NAME "Fretwire"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx Instrument
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(Fretwire PRIVATE ${FRETWIRE_SOURCES})

target_compile_definitions(Fretwire PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Fretwire PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

# ---- Dev tool: renders every preset (MIDI and synthetic-guitar), checks for
# silence/NaN/clipping/duplicates, tests the pitch tracker, calibrates preset
# loudness and snapshots the editor in every theme.
option(FRETWIRE_BUILD_TOOLS "Build the offline render/test tool" OFF)
if(FRETWIRE_BUILD_TOOLS)
    juce_add_console_app(FretwireRender PRODUCT_NAME "FretwireRender")
    target_sources(FretwireRender PRIVATE tools/Render.cpp ${FRETWIRE_SOURCES})
    target_compile_definitions(FretwireRender PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 FRETWIRE_NO_PLUGIN_ENTRY=1)
    target_link_libraries(FretwireRender PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
endif()
