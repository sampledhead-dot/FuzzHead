cmake_minimum_required(VERSION 3.22)
project(FuzzHead VERSION 1.0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "macOS mínimo")

# JUCE: usa una copia local (-DJUCE_DIR=/ruta/JUCE) o la descarga automáticamente
set(JUCE_DIR "" CACHE PATH "Ruta a una copia local de JUCE")
if(JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 9.0.5
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

set(FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND FORMATS AU)
endif()

juce_add_plugin(FuzzHead
    COMPANY_NAME "Sampled Head"
    PRODUCT_NAME "FuzzHead"
    BUNDLE_ID "com.sampledhead.fuzzhead"
    PLUGIN_MANUFACTURER_CODE Smhd
    PLUGIN_CODE Fzhd
    FORMATS ${FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    VST3_CATEGORIES Fx Distortion
    AU_MAIN_TYPE kAudioUnitType_Effect
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "FuzzHead necesita la entrada de audio para procesar tu guitarra o bajo."
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(FuzzHead PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(FuzzHead PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(FuzzHead
    PRIVATE juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

# Pruebas offline (opcional): cmake -DFUZZHEAD_BUILD_TESTS=ON
option(FUZZHEAD_BUILD_TESTS "Compilar la app de pruebas" OFF)
if(FUZZHEAD_BUILD_TESTS)
    juce_add_console_app(FuzzHeadTests PRODUCT_NAME "FuzzHeadTests")
    target_sources(FuzzHeadTests PRIVATE tests/TestMain.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
    target_compile_definitions(FuzzHeadTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1)
    target_link_libraries(FuzzHeadTests PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
endif()
