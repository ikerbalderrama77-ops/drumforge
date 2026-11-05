cmake_minimum_required(VERSION 3.22)
project(DRUMFORGE VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(DRUMFORGE_COPY_AFTER_BUILD "Instalar el VST3 en la carpeta del sistema tras compilar" ON)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(DRUMFORGE
    COMPANY_NAME "FlowProducer"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD ${DRUMFORGE_COPY_AFTER_BUILD}
    PLUGIN_MANUFACTURER_CODE FlwP
    PLUGIN_CODE Dfrg
    FORMATS VST3 Standalone
    PRODUCT_NAME "DRUMFORGE")

set(DF_SOURCES
    Source/PluginProcessor.h Source/PluginProcessor.cpp
    Source/PluginEditor.h Source/PluginEditor.cpp)

target_sources(DRUMFORGE PRIVATE ${DF_SOURCES})

target_compile_definitions(DRUMFORGE PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(DRUMFORGE
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_gui_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---- Tests (ctest) ----
enable_testing()
juce_add_console_app(DrumForgeTests PRODUCT_NAME "DrumForgeTests")
target_sources(DrumForgeTests PRIVATE
    Tests/Tests.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
target_compile_definitions(DrumForgeTests PRIVATE
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1)
target_link_libraries(DrumForgeTests PRIVATE
    juce::juce_audio_processors juce::juce_gui_basics
    juce::juce_recommended_config_flags)
add_test(NAME DrumForgeTests COMMAND DrumForgeTests)
