cmake_minimum_required(VERSION 3.22)

project(HumHousePitch VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Static MSVC runtime so the VST3 and the Standalone .exe load on a fresh
# Windows box without the VC++ redistributable.
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "" FORCE)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wno-unused-parameter)
endif()

include(FetchContent)

set(JUCE_VERSION 8.0.4 CACHE STRING "JUCE version to fetch")
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        ${JUCE_VERSION}
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

set(HHPS_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND HHPS_FORMATS AU)
endif()

juce_add_plugin(HumHousePitch
    COMPANY_NAME              "HumHouse"
    BUNDLE_ID                 "com.humhouse.humhousepitchshifter"
    PLUGIN_MANUFACTURER_CODE  Hhm1
    PLUGIN_CODE               HhP1
    FORMATS                   ${HHPS_FORMATS}
    PRODUCT_NAME              "HumHouse Pitch Shifter"
    IS_SYNTH                  FALSE
    NEEDS_MIDI_INPUT          FALSE
    NEEDS_MIDI_OUTPUT         FALSE
    IS_MIDI_EFFECT            FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD   FALSE
    VST3_CATEGORIES           "Fx" "Pitch Shift"
    AU_MAIN_TYPE              "kAudioUnitType_Effect"
)

juce_generate_juce_header(HumHousePitch)

target_sources(HumHousePitch PRIVATE
    SourcePS/PitchProcessor.cpp
    SourcePS/PitchProcessor.h
    SourcePS/PitchEditor.cpp
    SourcePS/PitchEditor.h
    SourcePS/PitchLookAndFeel.h
    SourcePS/PitchShiftEngine.cpp
    SourcePS/PitchShiftEngine.h
)

target_compile_definitions(HumHousePitch PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(HumHousePitch PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_plugin_client
    juce::juce_dsp
    juce::juce_gui_extra
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

# --- Tests -------------------------------------------------------------------
# The engine has no JUCE dependency, so the DSP guarantees (pitch accuracy,
# unity passthrough, doubler decorrelation) test as a plain executable.
enable_testing()

add_executable(HHPSEngineTests
    tests/EngineTests.cpp
    SourcePS/PitchShiftEngine.cpp)
target_compile_features(HHPSEngineTests PRIVATE cxx_std_17)
add_test(NAME HHPSEngineTests COMMAND HHPSEngineTests)

# The processor test needs the real plug-in: parameters, preset recall and the
# host state round-trip.
juce_add_console_app(HHPSProcessorTests PRODUCT_NAME "HHPSProcessorTests")
juce_generate_juce_header(HHPSProcessorTests)
target_sources(HHPSProcessorTests PRIVATE
    tests/ProcessorTests.cpp
    SourcePS/PitchProcessor.cpp
    SourcePS/PitchEditor.cpp
    SourcePS/PitchShiftEngine.cpp)
target_compile_definitions(HHPSProcessorTests PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_STANDALONE_APPLICATION=1
    JUCE_MODAL_LOOPS_PERMITTED=1
    JucePlugin_Name="HumHouse Pitch Shifter"
    JucePlugin_IsSynth=0
    JucePlugin_WantsMidiInput=0
    JucePlugin_ProducesMidiOutput=0
    JucePlugin_IsMidiEffect=0
    JucePlugin_VersionString="${PROJECT_VERSION}"
    JucePlugin_ManufacturerCode=0x48686d31
    JucePlugin_PluginCode=0x48685031)
target_link_libraries(HHPSProcessorTests PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_gui_extra
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
add_test(NAME HHPSProcessorTests COMMAND HHPSProcessorTests)
