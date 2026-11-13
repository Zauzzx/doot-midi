cmake_minimum_required(VERSION 3.22)

# Link the C++ runtime statically so the plugin works without installing
# the Visual C++ Redistributable.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(DootMidi VERSION 0.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(DootMidi
    COMPANY_NAME "Zauz"
    BUNDLE_ID "com.zauz.dootmidi"
    PLUGIN_MANUFACTURER_CODE Zauz
    PLUGIN_CODE Doot
    PRODUCT_NAME "Doot MIDI"
    FORMATS VST3 Standalone
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx Analyzer
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(DootMidi PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/NoteView.cpp
    Source/TakeRecorder.cpp
    Source/TakeResult.cpp
    Source/Engine/PitchTracker.cpp
    Source/Engine/NoteSegmenter.cpp)

target_compile_definitions(DootMidi PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    DOOT_VERSION_STRING="${PROJECT_VERSION}")

target_link_libraries(DootMidi
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Engine tests (plain C++, no JUCE): cmake -DDOOT_BUILD_TESTS=ON
option(DOOT_BUILD_TESTS "Build the detection engine tests" OFF)
if(DOOT_BUILD_TESTS)
    add_executable(engine_test
        tests/engine_test.cpp
        Source/Engine/PitchTracker.cpp
        Source/Engine/NoteSegmenter.cpp)
endif()
