cmake_minimum_required(VERSION 3.22)
project(Tank VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.13
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

set(CLAP_JUCE_EXTENSIONS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    clap-juce-extensions
    GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git
    GIT_TAG        main
)
FetchContent_MakeAvailable(clap-juce-extensions)

set(TANK_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND TANK_FORMATS AU)
endif()

juce_add_plugin(Tank
    COMPANY_NAME                "Spellbound"
    PLUGIN_MANUFACTURER_CODE    Spbd
    PLUGIN_CODE                 Tank
    FORMATS                     ${TANK_FORMATS}
    PRODUCT_NAME                "Tank"
    BUNDLE_ID                   "com.spellbound.tank"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    DESCRIPTION                 "Sidechain pumping compressor"
)

target_sources(Tank PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/DSP/BandpassFilter.h
    Source/DSP/RmsDetector.h
    Source/DSP/LookaheadDelay.h
)

target_compile_definitions(Tank PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_include_directories(Tank PRIVATE Source/)

target_compile_features(Tank PUBLIC cxx_std_20)

target_link_libraries(Tank
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_plugin_client
        juce::juce_dsp
        clap_juce_extensions
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

clap_juce_extensions_plugin(
    TARGET      Tank
    CLAP_ID     "com.spellbound.tank"
    CLAP_FEATURES audio-effect utility compressor
    CLAP_PROCESS_EVENTS_RESOLUTION_SAMPLES 64
)

# ── Unit tests (no JUCE dependency) ──────────────────────────────────────────
enable_testing()

add_executable(test_bandpassfilter
    Tests/test_bandpassfilter.cpp
)
target_include_directories(test_bandpassfilter PRIVATE Source/ Tests/)
target_compile_features(test_bandpassfilter PRIVATE cxx_std_20)
add_test(NAME BandpassFilter COMMAND test_bandpassfilter)

add_executable(test_rmsdetector
    Tests/test_rmsdetector.cpp
)
target_include_directories(test_rmsdetector PRIVATE Source/ Tests/)
target_compile_features(test_rmsdetector PRIVATE cxx_std_20)
add_test(NAME RmsDetector COMMAND test_rmsdetector)

add_executable(test_lookaheaddelay
    Tests/test_lookaheaddelay.cpp
)
target_include_directories(test_lookaheaddelay PRIVATE Source/ Tests/)
target_compile_features(test_lookaheaddelay PRIVATE cxx_std_20)
add_test(NAME LookaheadDelay COMMAND test_lookaheaddelay)

# ── Install rules ─────────────────────────────────────────────────────────────
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

set(_ARTS "${CMAKE_BINARY_DIR}/Tank_artefacts/${CMAKE_BUILD_TYPE}")

install(DIRECTORY  "${_ARTS}/VST3/Tank.vst3"
        DESTINATION VST3
        COMPONENT   Runtime
        USE_SOURCE_PERMISSIONS)

install(FILES      "${_ARTS}/CLAP/Tank.clap"
        DESTINATION CLAP
        COMPONENT   Runtime
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                    GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)

install(PROGRAMS   "${_ARTS}/Standalone/Tank"
        DESTINATION bin
        COMPONENT   Runtime)

install(PROGRAMS   "${CMAKE_CURRENT_SOURCE_DIR}/scripts/install.sh"
                   "${CMAKE_CURRENT_SOURCE_DIR}/scripts/uninstall.sh"
        DESTINATION .
        COMPONENT   Runtime)

# ── CPack (TGZ) ───────────────────────────────────────────────────────────────
set(CPACK_GENERATOR                 TGZ)
set(CPACK_PACKAGE_NAME              Tank)
set(CPACK_PACKAGE_VENDOR            Spellbound)
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Sidechain pumping compressor")
set(CPACK_PACKAGE_VERSION           ${PROJECT_VERSION})
set(CPACK_SYSTEM_NAME               linux-x86_64)
set(CPACK_PACKAGE_FILE_NAME         "Tank-${PROJECT_VERSION}-linux-x86_64")
set(CPACK_PACKAGING_INSTALL_PREFIX  "")
set(CPACK_STRIP_FILES               TRUE)
set(CPACK_PACKAGE_CHECKSUM          SHA256)
set(CPACK_INSTALL_CMAKE_PROJECTS
    "${CMAKE_BINARY_DIR};${PROJECT_NAME};Runtime;/")
include(CPack)
