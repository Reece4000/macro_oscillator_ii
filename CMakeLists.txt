cmake_minimum_required(VERSION 3.22)

project(MacroOscII VERSION 0.1.0 LANGUAGES C CXX)

if (NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(JUCE_DIR "/Users/reecemitchell/Programming/JUCE" CACHE PATH "Path to a JUCE checkout")
if (NOT EXISTS "${JUCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "JUCE_DIR must point to a JUCE checkout containing CMakeLists.txt")
endif()

add_subdirectory("${JUCE_DIR}" JUCE)

juce_add_binary_data(MacroOscIIAssets
    SOURCES
        Source/Assets/DSEG14Modern-Regular.ttf
        Source/Assets/Geist-Regular.ttf
        Source/Assets/Geist-Medium.ttf
        Source/Assets/GeistMono-Regular.ttf)

set(MUTABLE_PLAITS_SOURCES
    Source/ThirdParty/Mutable/plaits/dsp/chords/chord_bank.cc
    Source/ThirdParty/Mutable/plaits/dsp/engine/additive_engine.cc
    Source/ThirdParty/Mutable/plaits/dsp/engine/bass_drum_engine.cc
    Source/ThirdParty/Mutable/plaits/dsp/engine/chord_engine.cc
    Source/ThirdParty/Mutable/plaits/dsp/engine/fm_engine.cc
    Source/ThirdParty/Mutable/plaits/dsp/engine/grain_engine.cc
    Source/ThirdParty/Mutable/plaits/dsp/engine/hi_hat_engine.cc
    Source/ThirdParty/Mutable/plaits/dsp/engine/modal_engine.cc
    Source/ThirdParty/Mutable/plaits/dsp/engine/noise_engine.cc
    Source/ThirdParty/Mutable/plaits/dsp/engine/particle_engine.cc
    Source/ThirdParty/Mutable/plaits/dsp/engine/snare_drum_engine.cc
    Source/ThirdParty/Mutable/plaits/dsp/engine/speech_engine.cc
    Source/ThirdParty/Mutable/plaits/dsp/engine/string_engine.cc
    Source/ThirdParty/Mutable/plaits/dsp/engine/swarm_engine.cc
    Source/ThirdParty/Mutable/plaits/dsp/engine/virtual_analog_engine.cc
    Source/ThirdParty/Mutable/plaits/dsp/engine/waveshaping_engine.cc
    Source/ThirdParty/Mutable/plaits/dsp/engine/wavetable_engine.cc
    Source/ThirdParty/Mutable/plaits/dsp/engine2/chiptune_engine.cc
    Source/ThirdParty/Mutable/plaits/dsp/engine2/phase_distortion_engine.cc
    Source/ThirdParty/Mutable/plaits/dsp/engine2/six_op_engine.cc
    Source/ThirdParty/Mutable/plaits/dsp/engine2/string_machine_engine.cc
    Source/ThirdParty/Mutable/plaits/dsp/engine2/virtual_analog_vcf_engine.cc
    Source/ThirdParty/Mutable/plaits/dsp/engine2/wave_terrain_engine.cc
    Source/ThirdParty/Mutable/plaits/dsp/fm/algorithms.cc
    Source/ThirdParty/Mutable/plaits/dsp/fm/dx_units.cc
    Source/ThirdParty/Mutable/plaits/dsp/physical_modelling/modal_voice.cc
    Source/ThirdParty/Mutable/plaits/dsp/physical_modelling/resonator.cc
    Source/ThirdParty/Mutable/plaits/dsp/physical_modelling/string.cc
    Source/ThirdParty/Mutable/plaits/dsp/physical_modelling/string_voice.cc
    Source/ThirdParty/Mutable/plaits/dsp/speech/lpc_speech_synth.cc
    Source/ThirdParty/Mutable/plaits/dsp/speech/lpc_speech_synth_controller.cc
    Source/ThirdParty/Mutable/plaits/dsp/speech/lpc_speech_synth_phonemes.cc
    Source/ThirdParty/Mutable/plaits/dsp/speech/lpc_speech_synth_words.cc
    Source/ThirdParty/Mutable/plaits/dsp/speech/naive_speech_synth.cc
    Source/ThirdParty/Mutable/plaits/dsp/speech/sam_speech_synth.cc
    Source/ThirdParty/Mutable/plaits/dsp/voice.cc
    Source/ThirdParty/Mutable/plaits/resources.cc
    Source/ThirdParty/Mutable/stmlib/dsp/atan.cc
    Source/ThirdParty/Mutable/stmlib/dsp/units.cc
    Source/ThirdParty/Mutable/stmlib/utils/random.cc)

# TEST selects Plaits' desktop user-data stub instead of the module's flash access.
set_source_files_properties(
    Source/ThirdParty/Mutable/plaits/dsp/voice.cc
    PROPERTIES COMPILE_DEFINITIONS TEST)

set(VOICE_EFFECTS_SOURCES
    Source/DSP/VoiceEffects.cpp
    Source/DSP/NativeFilterProcessor.cpp
    Source/DSP/SamplerSvfFilter.cpp
    Source/DSP/NativeDistortionProcessor.cpp
    Source/DSP/NativeReductionProcessor.cpp
    Source/DSP/Fx/DistortionKernelBank.cpp
    Source/DSP/Fx/DistortionAmountProfiles.cpp)

set(AIRWINDOWS_DISTORTION_SOURCES
    Source/ThirdParty/Airwindows/Drive.cpp
    Source/ThirdParty/Airwindows/Fracture.cpp
    Source/ThirdParty/Airwindows/ToTape9.cpp
    Source/ThirdParty/Airwindows/IronOxideClassic2.cpp
    Source/ThirdParty/Airwindows/Density3.cpp
    Source/ThirdParty/Airwindows/Spiral2.cpp
    Source/ThirdParty/Airwindows/Tube2.cpp
    Source/ThirdParty/Airwindows/Mackity.cpp
    Source/ThirdParty/Airwindows/Coils2.cpp
    Source/ThirdParty/Airwindows/FireAmp.cpp
    Source/ThirdParty/Airwindows/DrumSlam.cpp
    Source/ThirdParty/Airwindows/BitGlitter.cpp
    Source/ThirdParty/Airwindows/DeBez.cpp
)

if (CMAKE_CXX_COMPILER_ID MATCHES "AppleClang|Clang|GNU")
    set_source_files_properties(${MUTABLE_PLAITS_SOURCES} ${AIRWINDOWS_DISTORTION_SOURCES}
        PROPERTIES
            COMPILE_OPTIONS "-w")
endif()

juce_add_plugin(MacroOscII
    COMPANY_NAME "Reece Mitchell"
    PRODUCT_NAME "Macro OSC II"
    BUNDLE_ID "com.reecemitchell.macroosc2"
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/Source/Assets/AppIcon.png"
    PLUGIN_MANUFACTURER_CODE Rmco
    PLUGIN_CODE Mos2
    FORMATS VST3 Standalone
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE)

target_sources(MacroOscII
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/Controls.cpp
        Source/WaveformDisplay.cpp
        Source/MsegEditor.cpp
        Source/EffectsDisplays.cpp
        Source/DSP/PlaitsVoice.cpp
        Source/DSP/PlaitsSynthesiser.cpp
        ${MUTABLE_PLAITS_SOURCES}
        ${VOICE_EFFECTS_SOURCES}
        ${AIRWINDOWS_DISTORTION_SOURCES})

target_include_directories(MacroOscII
    PRIVATE
        Source)

target_include_directories(MacroOscII
    SYSTEM PRIVATE
        Source/ThirdParty/Mutable)

target_compile_definitions(MacroOscII
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(MacroOscII
    PRIVATE
        MacroOscIIAssets
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

if (TARGET MacroOscII_Standalone)
    target_sources(MacroOscII_Standalone
        PRIVATE
            Source/StandaloneApp.cpp)

    target_compile_definitions(MacroOscII_Standalone
        PRIVATE
            JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1)

    # The app reaches the processor to offer audio settings from the plug-in's menu.
    target_include_directories(MacroOscII_Standalone PRIVATE Source)
    target_include_directories(MacroOscII_Standalone SYSTEM PRIVATE Source/ThirdParty/Mutable)
endif()

include(CTest)
if (BUILD_TESTING)
    add_executable(MacroOscIIDspTests Tests/DspTests.cpp Tests/PolyphonyTests.cpp)
    target_include_directories(MacroOscIIDspTests PRIVATE Source)
    target_include_directories(MacroOscIIDspTests SYSTEM PRIVATE Source/ThirdParty/Mutable "${JUCE_DIR}/modules")
    target_link_libraries(MacroOscIIDspTests PRIVATE MacroOscII)
    add_test(NAME macro_osc_ii_dsp COMMAND MacroOscIIDspTests)

    # Load the actual bundle so keyboard regressions also exercise MIDI's VST3
    # conversion, parameter delivery, and the plugin's public process callback.
    add_executable(MacroOscIIVst3Tests Tests/Vst3Tests.cpp)
    target_compile_definitions(MacroOscIIVst3Tests PRIVATE
        JUCE_PLUGINHOST_VST3=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(MacroOscIIVst3Tests PRIVATE juce::juce_audio_processors_headless)
    add_dependencies(MacroOscIIVst3Tests MacroOscII_VST3)
    add_test(NAME macro_osc_ii_vst3 COMMAND MacroOscIIVst3Tests
        "$<TARGET_FILE_DIR:MacroOscII_VST3>/../..")
endif()
