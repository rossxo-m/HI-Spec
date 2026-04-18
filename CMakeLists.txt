cmake_minimum_required(VERSION 3.22)

# CMake 4 removed implicit support for min-versions below 3.5. Some transitive
# deps pulled via FetchContent still declare low minimums; allow them.
if(NOT DEFINED CMAKE_POLICY_VERSION_MINIMUM)
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)
endif()

project(HISpec VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

include(FetchContent)

FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(HISpec
    COMPANY_NAME "HI"
    PLUGIN_MANUFACTURER_CODE HiSp
    PLUGIN_CODE HISP
    FORMATS VST3 Standalone
    PRODUCT_NAME "HI-Spec"
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_SYNTH FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(HISpec PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Parameters/ParameterLayout.cpp
    Source/DSP/HilbertFilterbank.cpp
    Source/DSP/SpectralDelay.cpp
    Source/DSP/Vocoder.cpp
    Source/DSP/SpectralFilter.cpp
    Source/DSP/Graph.cpp)

target_include_directories(HISpec PRIVATE Source)

target_compile_definitions(HISpec PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(HISpec
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

enable_testing()
add_subdirectory(tests)
