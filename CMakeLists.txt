cmake_minimum_required(VERSION 3.22)

project(VowelWorm VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS version")
endif()

option(VW_BUILD_PLUGIN "Build the JUCE plug-ins" ON)
option(VW_BUILD_TESTS  "Build engine tests and the offline renderer" ON)
option(VW_BUILD_INSTRUMENT "Also build the instrument + sidechain variant" ON)
set(VW_JUCE_DIR "" CACHE PATH "Path to a local JUCE checkout (leave empty to download JUCE 8.0.10)")
set(VW_FORMATS "VST3;AU;Standalone" CACHE STRING "Plug-in formats")

set(VW_ENGINE_SOURCES
    Source/Engine/Engine.cpp)

# ------------------------------------------------------------------ engine tests
if(VW_BUILD_TESTS)
    add_executable(vw_tests Tests/EngineTests.cpp ${VW_ENGINE_SOURCES})
    target_include_directories(vw_tests PRIVATE Source)
    add_executable(vw_render Tools/Render.cpp ${VW_ENGINE_SOURCES})
    target_include_directories(vw_render PRIVATE Source)
    if(NOT MSVC)
        target_compile_options(vw_tests PRIVATE -O2 -Wall -Wextra)
        target_compile_options(vw_render PRIVATE -O2 -Wall -Wextra)
    endif()
    enable_testing()
    add_test(NAME engine COMMAND vw_tests)
endif()

# ------------------------------------------------------------------ plug-ins
if(VW_BUILD_PLUGIN)
    if(VW_JUCE_DIR)
        add_subdirectory(${VW_JUCE_DIR} ${CMAKE_BINARY_DIR}/JUCE)
    else()
        include(FetchContent)
        FetchContent_Declare(JUCE
            GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
            GIT_TAG 8.0.10
            GIT_SHALLOW TRUE)
        FetchContent_MakeAvailable(JUCE)
    endif()

    if(NOT APPLE)
        list(REMOVE_ITEM VW_FORMATS AU)
    endif()

    function(vw_add_plugin target product code isInstrument)
        if(isInstrument)
            set(category Instrument)
            set(vst3cat Instrument Synth)
            set(synth TRUE)
            set(auType kAudioUnitType_MusicDevice)
        else()
            set(category Fx)
            set(vst3cat Fx Pitch Shift)
            set(synth FALSE)
            set(auType kAudioUnitType_MusicEffect)  # audio in + MIDI in
        endif()

        juce_add_plugin(${target}
            PRODUCT_NAME "${product}"
            COMPANY_NAME "Joe Caple"
            BUNDLE_ID "com.joecaple.${target}"
            PLUGIN_MANUFACTURER_CODE Jcap
            PLUGIN_CODE ${code}
            FORMATS ${VW_FORMATS}
            IS_SYNTH ${synth}
            NEEDS_MIDI_INPUT TRUE
            NEEDS_MIDI_OUTPUT FALSE
            IS_MIDI_EFFECT FALSE
            EDITOR_WANTS_KEYBOARD_FOCUS FALSE
            VST3_CATEGORIES ${vst3cat}
            AU_MAIN_TYPE ${auType}
            MICROPHONE_PERMISSION_ENABLED TRUE
            MICROPHONE_PERMISSION_TEXT "VowelWorm needs the microphone to harmonise your voice."
            COPY_PLUGIN_AFTER_BUILD FALSE)

        target_sources(${target} PRIVATE
            Source/Plugin/PluginProcessor.cpp
            Source/Plugin/PluginEditor.cpp
            ${VW_ENGINE_SOURCES})
        target_include_directories(${target} PRIVATE Source)

        target_compile_definitions(${target} PUBLIC
            VW_INSTRUMENT=$<IF:$<BOOL:${isInstrument}>,1,0>
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JUCE_VST3_CAN_REPLACE_VST2=0
            JUCE_DISPLAY_SPLASH_SCREEN=0)

        target_link_libraries(${target}
            PRIVATE
                juce::juce_audio_utils
                juce::juce_audio_processors
            PUBLIC
                juce::juce_recommended_config_flags
                juce::juce_recommended_lto_flags
                juce::juce_recommended_warning_flags)
    endfunction()

    vw_add_plugin(VowelWorm "VowelWorm" Vwrm FALSE)
    if(VW_BUILD_INSTRUMENT)
        vw_add_plugin(VowelWormInst "VowelWorm Inst" Vwri TRUE)
    endif()
endif()

# ------------------------------------------------------------------ dev tool: UI snapshots
option(VW_BUILD_SNAPSHOT "Build the UI snapshot tool (development only)" OFF)
if(VW_BUILD_PLUGIN AND VW_BUILD_SNAPSHOT)
    juce_add_console_app(vw_snapshot PRODUCT_NAME "vw_snapshot")
    target_sources(vw_snapshot PRIVATE Tools/Snapshot.cpp Source/Plugin/PluginProcessor.cpp Source/Plugin/PluginEditor.cpp ${VW_ENGINE_SOURCES})
    target_include_directories(vw_snapshot PRIVATE Source)
    target_compile_definitions(vw_snapshot PRIVATE VW_INSTRUMENT=0 JucePlugin_Name="VowelWorm" JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(vw_snapshot PRIVATE juce::juce_audio_utils juce::juce_audio_processors juce::juce_recommended_config_flags)
endif()
