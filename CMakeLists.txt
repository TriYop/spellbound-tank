cmake_minimum_required(VERSION 3.22)
project(Tank VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

# DPF has no tagged releases -- pin an exact commit SHA on `main` instead.
# Same commit Hex/Pugilist use (see cmake/patches/*.patch for why each patch
# is needed; neither has been reported upstream yet).
set(AUDIOPLUGINS_DPF_GIT_TAG "4238e1c7f0351bbe488d79f0899c540543ac7583" CACHE STRING "Pinned DPF commit")

# CAVEAT (CMake FetchContent): the patch step only runs on a *fresh*
# population of the source dir. If build/_deps/dpf-src already exists from
# an earlier configure, a plain reconfigure will NOT re-run it -- delete
# build/ (or at least build/_deps/dpf-*) when you need to be sure the
# patches actually took effect. cmake/apply_patch.cmake is idempotent (it
# skips a patch that's already applied), so a re-run is harmless, but that
# is not a substitute for a clean re-fetch.
FetchContent_Declare(dpf
    GIT_REPOSITORY https://github.com/DISTRHO/DPF.git
    GIT_TAG        ${AUDIOPLUGINS_DPF_GIT_TAG}
    GIT_SHALLOW    TRUE
    PATCH_COMMAND  ${CMAKE_COMMAND}
                   -DPATCH_FILE=${CMAKE_CURRENT_SOURCE_DIR}/cmake/patches/dpf-clap-state-chunked-read.patch
                   -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/apply_patch.cmake
            COMMAND ${CMAKE_COMMAND}
                   -DPATCH_FILE=${CMAKE_CURRENT_SOURCE_DIR}/cmake/patches/dpf-clap-activate-latency.patch
                   -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/apply_patch.cmake
)
FetchContent_MakeAvailable(dpf)

set(TANK_DPF_TARGETS vst3 clap lv2)
if(APPLE)
    list(APPEND TANK_DPF_TARGETS au)
endif()

dpf_add_plugin(Tank
    TARGETS ${TANK_DPF_TARGETS}
    UI_TYPE opengl
    USE_FILE_BROWSER TRUE
    FILES_DSP
        Source/TankPluginAdapter.cpp
    FILES_UI
        Source/TankUI.cpp
)

target_include_directories(Tank PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/Source")

# Same DPF WebViewImpl.cpp / GNU `typeof` workaround Hex needs (see Hex's
# CMakeLists.txt for the full explanation) -- unconditionally compiled in by
# a DPF bug regardless of USE_WEB_VIEW, fails under strict ISO C++.
if(TARGET dgl-opengl)
    set_target_properties(dgl-opengl PROPERTIES CXX_EXTENSIONS ON)
endif()

# ── Shared HUI/DSP library ───────────────────────────────────────────────────
# ORDERING (load-bearing): must come after dpf_add_plugin(Tank ...), which is
# what lazily creates DPF's dgl-opengl target that AudioPluginsCommon::hui_dgl
# is guarded on. See Hex's CMakeLists.txt for the full explanation.
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

FetchContent_Declare(AudioPluginsCommon
    GIT_REPOSITORY https://github.com/TriYop/spellbound-common.git
    GIT_TAG        v0.3.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(AudioPluginsCommon)

# ParameterSmoother (gain smoothing) and MeterTransport (audio-thread ->
# UI-thread GR value, see TankPluginAdapter.h) both need a real link, not
# just an include path.
target_link_libraries(Tank PUBLIC AudioPluginsCommon::io)

# Linked into `Tank-ui` (not `Tank`): dpf_add_plugin() splits the plugin into
# a common `Tank` static lib plus `Tank-dsp`/`Tank-ui`; TankUI.cpp is the
# only consumer of the widgets and lives in `Tank-ui`. Same reasoning as Hex.
target_link_libraries(Tank-ui PRIVATE AudioPluginsCommon::hui_dgl AudioPluginsCommon::presets)

# ── Unit tests (no JUCE/DPF dependency) ──────────────────────────────────────
enable_testing()

add_executable(test_bandpassfilter Tests/test_bandpassfilter.cpp)
target_include_directories(test_bandpassfilter PRIVATE Source/ Tests/)
target_compile_features(test_bandpassfilter PRIVATE cxx_std_20)
add_test(NAME BandpassFilter COMMAND test_bandpassfilter)

add_executable(test_rmsdetector Tests/test_rmsdetector.cpp)
target_include_directories(test_rmsdetector PRIVATE Source/ Tests/)
target_compile_features(test_rmsdetector PRIVATE cxx_std_20)
add_test(NAME RmsDetector COMMAND test_rmsdetector)

add_executable(test_lookaheaddelay Tests/test_lookaheaddelay.cpp)
target_include_directories(test_lookaheaddelay PRIVATE Source/ Tests/)
target_compile_features(test_lookaheaddelay PRIVATE cxx_std_20)
add_test(NAME LookaheadDelay COMMAND test_lookaheaddelay)

add_executable(test_duckingenvelope Tests/test_duckingenvelope.cpp)
target_include_directories(test_duckingenvelope PRIVATE Source/ Tests/)
target_compile_features(test_duckingenvelope PRIVATE cxx_std_20)
add_test(NAME DuckingEnvelope COMMAND test_duckingenvelope)

add_executable(test_sidechainguard Tests/test_sidechainguard.cpp)
target_include_directories(test_sidechainguard PRIVATE Source/ Tests/)
target_compile_features(test_sidechainguard PRIVATE cxx_std_20)
add_test(NAME SidechainGuard COMMAND test_sidechainguard)

# Task 3 (SidechainGuard) and Task 7 (FactoryPresets) each append their own
# add_executable/add_test block here once they create their test files --
# deliberately not included yet, so this task's own build-verification step
# (Step 9 below) has a CMakeLists.txt that only references files that exist
# at this point in the plan.

# ── Install rules ─────────────────────────────────────────────────────────────
# DPF writes all format outputs under a flat bin/ dir -- no Standalone
# target: Tank is an effect, and this workspace only requires Standalone for
# instruments (see AudioPlugins/CLAUDE.md).
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

set(_BIN "${CMAKE_BINARY_DIR}/bin")

install(DIRECTORY  "${_BIN}/Tank.vst3"
        DESTINATION VST3
        COMPONENT   Runtime
        USE_SOURCE_PERMISSIONS)

# CLAP is a flat shared-library file on Linux/Windows but a bundle
# *directory* on macOS (like VST3/AU) -- see Hex's CMakeLists.txt.
if(APPLE)
    install(DIRECTORY  "${_BIN}/Tank.clap"
            DESTINATION CLAP
            COMPONENT   Runtime
            USE_SOURCE_PERMISSIONS)
else()
    install(FILES      "${_BIN}/Tank.clap"
            DESTINATION CLAP
            COMPONENT   Runtime
            PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                        GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
endif()

install(DIRECTORY  "${_BIN}/Tank.lv2"
        DESTINATION LV2
        COMPONENT   Runtime
        USE_SOURCE_PERMISSIONS)

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
