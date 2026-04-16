# SPDX-License-Identifier: AGPL-3.0-or-later

cmake_minimum_required(VERSION 3.22)

# Plugin and JUCE ProjectInfo::* use this as the single version source (see juce_add_plugin VERSION default).
project(APCX VERSION 1.0.0 LANGUAGES CXX)

# Helps clangd when using Ninja or Makefile generators (not emitted for VS generator).
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Set C++ standard to 20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add JUCE as a subdirectory
add_subdirectory(JUCE)

# Create the plugin target
juce_add_plugin(APCX
    VERSION ${PROJECT_VERSION}
    COMPANY_NAME "Distant Nebula"
    BUNDLE_ID "com.distantnebula.apcx"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT TRUE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD TRUE
    VST3_COPY_DIR "${CMAKE_CURRENT_BINARY_DIR}/VST3"
    PLUGIN_CODE "APCX"
    PLUGIN_MANUFACTURER_CODE "DNEB"
    FORMATS VST3
    PRODUCT_NAME "APCX")

# Generate JuceHeader.h for compatibility
juce_generate_juce_header(APCX)

# Add source files
target_sources(APCX PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.hpp
    Source/PluginEditor.cpp
    Source/PluginEditor.hpp
    Source/MusicTheory.cpp
    Source/MusicTheory.hpp
    Source/ModifierButton.cpp
    Source/ModifierButton.hpp
    Source/NoteButton.cpp
    Source/NoteButton.hpp
    Source/StepButton.cpp
    Source/StepButton.hpp
    Source/NoteData.hpp
    Source/HardwareDisplay.hpp
)

# Link to JUCE modules
target_link_libraries(APCX PRIVATE
    juce::juce_audio_processors
    juce::juce_audio_plugin_client
    juce::juce_audio_utils
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_core
    juce::juce_data_structures
    juce::juce_events
)

# Set compile definitions
target_compile_definitions(APCX PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)
