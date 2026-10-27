# =============================================================================
#  SVT-AV2 - independent AV2 production encoder implementation
#
#  M4 scope: scalar All-Intra / low-delay P, 8-bit420 and 10-bit420/444,
#  multi-frame public API and decoder-exact internal reconstruction.
#
#  Design notes:
#   * C11 only, no external dependencies (no gtest / no third-party runtime).
#   * Ninja + MSVC on Windows, Ninja + GCC/Clang on Linux.
#   * AVM tools are used as an external oracle and are never built here.
# =============================================================================
cmake_minimum_required(VERSION 3.16)

project(SvtAv2
        VERSION 0.0.5
        DESCRIPTION "Independent AV2 production encoder implementation"
        LANGUAGES C)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# --- options -----------------------------------------------------------------
option(SVT_AV2_BUILD_APPS         "Build the SvtAv2EncApp executable"          ON)
option(SVT_AV2_BUILD_TESTS        "Build unit tests and register CTest cases"  ON)
option(SVT_AV2_BUILD_SHARED       "Also build a shared library variant"        ON)
option(SVT_AV2_ENABLE_SANITIZERS  "Enable ASan/UBSan (GCC/Clang only)"         OFF)
option(SVT_AV2_WARNINGS_AS_ERRORS "Treat compiler warnings as errors"          OFF)
set(SVT_AV2_ORACLE_DIR "" CACHE PATH "Directory containing the AVM reference decoder (avmdec)")

# --- build type --------------------------------------------------------------
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)

# --- language -----------------------------------------------------------------
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# --- output layout ------------------------------------------------------------
set(SVT_AV2_BIN_DIR "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${SVT_AV2_BIN_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

include(SvtAv2CompilerFlags)
include(SvtAv2Oracle)

if(MSVC)
    # The code base uses the standard C library API (fopen/fprintf/...).
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif()

enable_testing()

add_subdirectory(Source/Lib/Common)
# T008 owns Source/Lib/Prediction (minimal scalar intra prediction + residual).
# Depends on svt_av2_common (picture + block geometry from T007), so it must be
# added AFTER Common. Registered by 00 during M2 integration.
add_subdirectory(Source/Lib/Prediction)
# T010 owns Source/Lib/Quantization (fixed-QP quantize + decoder-path dequant).
# Registered by 00 during M2 integration; depends on Common for shared types.
add_subdirectory(Source/Lib/Quantization)
# T009 owns Source/Lib/Transform (scalar forward/inverse DCT_DCT, 8-bit, <=16x16).
# Registered by 00 during M2 integration; depends on Common for shared types.
add_subdirectory(Source/Lib/Transform)
# T012 owns Source/Lib/Reconstruction (clipped write / scratch / raw export) and
# Source/Lib/Encoder (ALL-INTRA per-frame chain driver). Reconstruction depends
# on Common; the Encoder frame core depends on Prediction + Reconstruction, so
# both must come AFTER those two. Registered by 00 during M2 integration.
add_subdirectory(Source/Lib/Reconstruction)
add_subdirectory(Source/Lib/Encoder)

# Dependency order matters: CMake resolves target_link_libraries against targets
# that already exist. encoder.c (Codec) now drives the M1 keyframe writer and
# links svt_av2_bitstream, so Bitstream and EntropyCoding must be added BEFORE
# Codec -- otherwise the link name matches no target, the include directories
# never propagate, and the build fails on "keyframe.h: No such file or directory".
#
# T003 owns Source/Lib/Bitstream (bit_writer/obu_writer, plus sequence_header
# contributed by T004). Registered here by 00 during M1 integration.
add_subdirectory(Source/Lib/Bitstream)
# T005 owns Source/Lib/EntropyCoding (AV2 symbol/range encoder). Registered by
# 00 during M1 integration; must precede Codec now that the encoder link uses it.
add_subdirectory(Source/Lib/EntropyCoding)
add_subdirectory(Source/Lib/Codec)

if(SVT_AV2_BUILD_APPS)
    add_subdirectory(app/SvtAv2EncApp)
endif()

if(SVT_AV2_BUILD_TESTS)
    # tests/CMakeLists.txt is owned and registered by T002. Until it lands, a
    # default (SVT_AV2_BUILD_TESTS=ON) configure must still succeed: guard the
    # add_subdirectory instead of failing on the missing file. Once T002 adds
    # the file the tests are picked up automatically, with no root change.
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
        add_subdirectory(tests)
    else()
        message(STATUS "SVT-AV2: tests/CMakeLists.txt not present yet; unit tests are not registered")
    endif()
endif()

# --- configuration summary ----------------------------------------------------
add_custom_target(svt_av2_summary
    COMMAND ${CMAKE_COMMAND} -E echo "----------------------------------------------------------"
    COMMAND ${CMAKE_COMMAND} -E echo "SVT-AV2 ${PROJECT_VERSION} configuration"
    COMMAND ${CMAKE_COMMAND} -E echo "  system          : ${CMAKE_SYSTEM_NAME} / ${CMAKE_SYSTEM_PROCESSOR}"
    COMMAND ${CMAKE_COMMAND} -E echo "  compiler        : ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}"
    COMMAND ${CMAKE_COMMAND} -E echo "  build type      : ${CMAKE_BUILD_TYPE}"
    COMMAND ${CMAKE_COMMAND} -E echo "  apps / tests    : ${SVT_AV2_BUILD_APPS} / ${SVT_AV2_BUILD_TESTS}"
    COMMAND ${CMAKE_COMMAND} -E echo "  shared library  : ${SVT_AV2_BUILD_SHARED}"
    COMMAND ${CMAKE_COMMAND} -E echo "  sanitizers      : ${SVT_AV2_ENABLE_SANITIZERS}"
    COMMAND ${CMAKE_COMMAND} -E echo "  AVM oracle      : ${SVT_AV2_ORACLE_STATUS}"
    COMMAND ${CMAKE_COMMAND} -E echo "  avmdec          : ${SVT_AV2_AVMDEC}"
    COMMAND ${CMAKE_COMMAND} -E echo "  bin dir         : ${SVT_AV2_BIN_DIR}"
    COMMAND ${CMAKE_COMMAND} -E echo "----------------------------------------------------------"
    VERBATIM)

message(STATUS "SVT-AV2: build type = ${CMAKE_BUILD_TYPE}, C compiler = ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}")
message(STATUS "SVT-AV2: AVM oracle = ${SVT_AV2_ORACLE_STATUS}")
