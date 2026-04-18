include(FetchContent)

FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.5.4)
FetchContent_MakeAvailable(Catch2)

add_executable(hispec_tests
    main.cpp
    ParameterLayoutTests.cpp
    HilbertFilterbankTests.cpp
    SpectralDelayTests.cpp
    ${CMAKE_SOURCE_DIR}/Source/Parameters/ParameterLayout.cpp
    ${CMAKE_SOURCE_DIR}/Source/DSP/HilbertFilterbank.cpp
    ${CMAKE_SOURCE_DIR}/Source/DSP/SpectralDelay.cpp)

target_include_directories(hispec_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/Source)

target_compile_definitions(hispec_tests PRIVATE
    JUCE_STANDALONE_APPLICATION=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_MODAL_LOOPS_PERMITTED=0)

target_link_libraries(hispec_tests
    PRIVATE
        Catch2::Catch2WithMain
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

target_compile_features(hispec_tests PRIVATE cxx_std_20)

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(CTest)
include(Catch)
catch_discover_tests(hispec_tests)
