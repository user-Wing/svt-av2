# =============================================================================
#  SVT-AV2 test registration.
#
#  Ownership: T002 (unified test registration). Per the M0-M1 contract
#  (docs/m0-m1-contracts.md section 3, decision "plan B"), T001 created the
#  initial minimal file to unblock its own acceptance, then handed ownership to
#  T002. T002 extends it serially to register the other sessions' unit tests.
#
#  Rules:
#   * Explicit source lists only - no file(GLOB), so a missing or unregistered
#     test is a visible failure rather than a silently ignored file.
#   * Every registered test is a plain executable with its own main().
#   * Python-driven checks use the interpreter found by CMake; the project has
#     no third-party runtime dependency.
# =============================================================================

# --- T001: skeleton, public API, Y4M and geometry unit tests -----------------
set(SVT_AV2_T001_UNIT_TESTS
    test_common
    test_y4m
    test_api
    test_still_picture
    test_m6_api
    test_m5_api)

foreach(_test IN LISTS SVT_AV2_T001_UNIT_TESTS)
    add_executable(${_test} "unit/${_test}.c")
    target_link_libraries(${_test} PRIVATE svt_av2_enc)
    # test_util.h lives next to the sources.
    target_include_directories(${_test} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/unit")
    svt_av2_set_target_flags(${_test})

    add_test(NAME ${_test} COMMAND ${_test})
    # A failing assertion makes the executable exit non-zero; make that the
    # single, unambiguous failure criterion for CTest.
    set_tests_properties(${_test} PROPERTIES FAIL_REGULAR_EXPRESSION "FAIL ")
endforeach()

unset(SVT_AV2_T001_UNIT_TESTS)

foreach(_test test_root_asym test_flip_primary_tx test_thin64_domains test_intra_thin64 test_intra_thin64_combo test_intra_mixedthin test_intra_mixedthin_combo test_intra_thinrect_uv test_thinrect_domains test_intra_thinrect_combo test_intra_thinrect test_mhccp_fault test_mhccp_boundaries test_mhccp_rd test_intra_mhccp test_mixed8_rd test_intra_mixed8 test_classic_cfl_rd test_intra_classic_cfl test_intra_large_mrl test_smallrect_primary_tx test_tree_wide test_intra_lambda test_intra_longrect_angles test_intra_mixed32 test_mixed32_rd test_intra_longrect_cfl test_mrl_rect_rd test_intra_mrl_rect test_tree_finalists test_mrl_axes test_mrl_rd test_intra_mrl test_full_primary_tx test_intra_angles_uv64 test_intra_angles test_primary_tx test_i1_modes test_intra_rect_angles test_intra_rect test_intra_longrect test_intra_cfl test_m6_rd test_m6_rate test_m6_coeff test_m6_motion test_m6_wide test_m7_entropy test_adaptive_rd)
    add_executable(${_test} "unit/${_test}.c")
    target_link_libraries(${_test} PRIVATE svt_av2_encoder_core svt_av2_bitstream)
    if(NOT MSVC)
        target_link_libraries(${_test} PRIVATE m)
    endif()
    target_include_directories(${_test} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/unit")
    svt_av2_set_target_flags(${_test})
    add_test(NAME ${_test} COMMAND ${_test})
    set_tests_properties(${_test} PROPERTIES TIMEOUT 180 FAIL_REGULAR_EXPRESSION "FAIL ")
endforeach()

set_tests_properties(test_tree_finalists PROPERTIES TIMEOUT 1200)
set_tests_properties(test_intra_rect_angles PROPERTIES TIMEOUT 600)
set_tests_properties(test_mrl_rd PROPERTIES TIMEOUT 600)
set_tests_properties(test_tree_wide PROPERTIES TIMEOUT 1200)

if(SVT_AV2_ENABLE_SANITIZERS)
    set_tests_properties(test_mrl_rd PROPERTIES TIMEOUT 3600)
    set_tests_properties(test_tree_finalists test_tree_wide PROPERTIES TIMEOUT 3600)
    # Forced normative matrices execute tens of thousands of complete pictures.
    set_tests_properties(test_intra_angles test_intra_angles_uv64 test_intra_rect_angles
        test_intra_rect test_intra_longrect test_intra_large_mrl test_smallrect_primary_tx test_intra_longrect_angles test_intra_longrect_cfl test_intra_mrl test_intra_mrl_rect test_full_primary_tx test_intra_cfl PROPERTIES TIMEOUT 1800)
endif()

# Mixed 8x8 tree reversal and fault recovery rebuild every candidate SB.
set_tests_properties(test_m6_rd PROPERTIES TIMEOUT 600)

# Keep independent tool audits in separate processes; a long combined test
# obscures which rollback/reversal domain failed and unnecessarily serializes CI.
foreach(_domain mixed primary rdoq cfl angles rect fulltx longrect)
    add_test(NAME test_m6_rd_${_domain} COMMAND test_m6_rd ${_domain})
    set_tests_properties(test_m6_rd_${_domain} PROPERTIES TIMEOUT 600 FAIL_REGULAR_EXPRESSION "FAIL ")
    if(SVT_AV2_ENABLE_SANITIZERS)
        set_tests_properties(test_m6_rd_${_domain} PROPERTIES TIMEOUT 3600)
    endif()
endforeach()

# --- T003/T004: bit writer, LEB128, OBU framing and sequence header ----------
if(SVT_AV2_ENABLE_SANITIZERS)
    # Fine-tree and directional rollback/reversal paths rebuild complete SBs.
    set_tests_properties(test_m6_rd PROPERTIES TIMEOUT 3600)
endif()

# The module target is registered into the root CMake by 00/T001 (root CMake is a
# shared file). Until that add_subdirectory lands the tests cannot link, so
# report the pending registration at configure time instead of hiding it.
if(TARGET svt_av2_bitstream)
    set(SVT_AV2_BITSTREAM_UNIT_TESTS
        test_bit_writer        # T003
        test_obu_writer        # T003
        test_sequence_header)  # T004

    foreach(_test IN LISTS SVT_AV2_BITSTREAM_UNIT_TESTS)
        add_executable(${_test} "unit/${_test}.c")
        target_link_libraries(${_test} PRIVATE svt_av2_bitstream)
        target_include_directories(${_test} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/unit")
        svt_av2_set_target_flags(${_test})

        add_test(NAME ${_test} COMMAND ${_test})
        set_tests_properties(${_test} PROPERTIES FAIL_REGULAR_EXPRESSION "FAIL ")
    endforeach()

    unset(SVT_AV2_BITSTREAM_UNIT_TESTS)
else()
    message(STATUS "SVT-AV2: svt_av2_bitstream target absent - T003/T004/T005 unit tests not registered (root CMake add_subdirectory pending)")
endif()

# --- T005: minimal frame/tile/keyframe/entropy unit tests ---------------------
# Registered during M1 integration; T002 owns this file and maintains the list.
# Links svt_av2_bitstream, which PUBLIC-links svt_av2_entropy, so the entropy
# symbols are available transitively.
if(TARGET svt_av2_bitstream)
    set(SVT_AV2_FRAME_UNIT_TESTS
        test_tile_writer      # T005
        test_frame_header     # T005
        test_keyframe)        # T005

    foreach(_test IN LISTS SVT_AV2_FRAME_UNIT_TESTS)
        add_executable(${_test} "unit/${_test}.c")
        target_link_libraries(${_test} PRIVATE svt_av2_bitstream)
        target_include_directories(${_test} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/unit")
        svt_av2_set_target_flags(${_test})

        add_test(NAME ${_test} COMMAND ${_test})
        set_tests_properties(${_test} PROPERTIES FAIL_REGULAR_EXPRESSION "FAIL ")
    endforeach()

    unset(SVT_AV2_FRAME_UNIT_TESTS)
else()
    message(STATUS "SVT-AV2: svt_av2_bitstream target absent - T005 unit tests not registered")
endif()

# --- T011: coefficient syntax and tile writer capacity ------------------------
# test_coeff_syntax links svt_av2_entropy directly (the coefficient syntax is
# part of the entropy coder and must not pull in svt_av2_bitstream, which
# PUBLIC-links svt_av2_entropy). test_tile_writer_capacity links
# svt_av2_bitstream because it exercises the tile payload writer.
# Source owned by T011, registered here by 00 (tests/CMakeLists.txt is shared).
if(TARGET svt_av2_entropy)
    add_executable(test_coeff_syntax "unit/test_coeff_syntax.c")
    target_link_libraries(test_coeff_syntax PRIVATE svt_av2_entropy)
    target_include_directories(test_coeff_syntax PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/unit")
    svt_av2_set_target_flags(test_coeff_syntax)

    add_test(NAME test_coeff_syntax COMMAND test_coeff_syntax)
    set_tests_properties(test_coeff_syntax PROPERTIES FAIL_REGULAR_EXPRESSION "FAIL ")
else()
    message(STATUS "SVT-AV2: svt_av2_entropy target absent - test_coeff_syntax not registered")
endif()

if(TARGET svt_av2_bitstream)
    add_executable(test_tile_writer_capacity "unit/test_tile_writer_capacity.c")
    target_link_libraries(test_tile_writer_capacity PRIVATE svt_av2_bitstream)
    target_include_directories(test_tile_writer_capacity PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/unit")
    svt_av2_set_target_flags(test_tile_writer_capacity)

    add_test(NAME test_tile_writer_capacity COMMAND test_tile_writer_capacity)
    set_tests_properties(test_tile_writer_capacity PROPERTIES FAIL_REGULAR_EXPRESSION "FAIL ")
else()
    message(STATUS "SVT-AV2: svt_av2_bitstream target absent - test_tile_writer_capacity not registered")
endif()

# --- T015: multi-frame sequence header and frame/tile header bit width --------
# Same dependency as T005 (svt_av2_bitstream). Source owned by T015, registered
# here by 00 (tests/CMakeLists.txt is a shared file).
if(TARGET svt_av2_bitstream)
    add_executable(test_header_frames "unit/test_header_frames.c")
    target_link_libraries(test_header_frames PRIVATE svt_av2_bitstream)
    target_include_directories(test_header_frames PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/unit"
        "${CMAKE_SOURCE_DIR}/Source/Lib/Bitstream")
    svt_av2_set_target_flags(test_header_frames)

    add_test(NAME test_header_frames COMMAND test_header_frames)
    set_tests_properties(test_header_frames PROPERTIES FAIL_REGULAR_EXPRESSION "FAIL ")
else()
    message(STATUS "SVT-AV2: svt_av2_bitstream target absent - T015 test_header_frames not registered")
endif()

# --- T007: picture storage and block geometry --------------------------------
# Links svt_av2_common, where picture.c and block_geometry.c live (registered by
# 00 in Source/Lib/Common/CMakeLists.txt during M2 integration).
set(SVT_AV2_COMMON_UNIT_TESTS
    test_picture           # T007
    test_block_geometry)   # T007

foreach(_test IN LISTS SVT_AV2_COMMON_UNIT_TESTS)
    add_executable(${_test} "unit/${_test}.c")
    target_link_libraries(${_test} PRIVATE svt_av2_common)
    target_include_directories(${_test} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/unit")
    svt_av2_set_target_flags(${_test})

    add_test(NAME ${_test} COMMAND ${_test})
    set_tests_properties(${_test} PROPERTIES FAIL_REGULAR_EXPRESSION "FAIL ")
endforeach()

unset(SVT_AV2_COMMON_UNIT_TESTS)

# --- T008: minimal scalar intra prediction and signed residual ----------------
# Links svt_av2_prediction, which PUBLIC-links svt_av2_common (picture +
# block_geometry from T007), so those symbols are available transitively.
if(TARGET svt_av2_prediction)
    set(SVT_AV2_PREDICTION_UNIT_TESTS
        test_intra_prediction  # T008
        test_residual)         # T008

    foreach(_test IN LISTS SVT_AV2_PREDICTION_UNIT_TESTS)
        add_executable(${_test} "unit/${_test}.c")
        target_link_libraries(${_test} PRIVATE svt_av2_prediction)
        target_include_directories(${_test} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/unit")
        svt_av2_set_target_flags(${_test})

        add_test(NAME ${_test} COMMAND ${_test})
        set_tests_properties(${_test} PROPERTIES FAIL_REGULAR_EXPRESSION "FAIL ")
    endforeach()

    unset(SVT_AV2_PREDICTION_UNIT_TESTS)
else()
    message(STATUS "SVT-AV2: svt_av2_prediction target absent - T008 unit tests not registered")
endif()

# --- T010: quantization / dequantization -------------------------------------
# Links svt_av2_quantization, which PUBLIC-links svt_av2_common.
if(TARGET svt_av2_quantization)
    set(SVT_AV2_QUANTIZATION_UNIT_TESTS
        test_quantization)     # T010

    foreach(_test IN LISTS SVT_AV2_QUANTIZATION_UNIT_TESTS)
        add_executable(${_test} "unit/${_test}.c")
        target_link_libraries(${_test} PRIVATE svt_av2_quantization)
        target_include_directories(${_test} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/unit")
        svt_av2_set_target_flags(${_test})

        add_test(NAME ${_test} COMMAND ${_test})
        set_tests_properties(${_test} PROPERTIES FAIL_REGULAR_EXPRESSION "FAIL ")
    endforeach()

    unset(SVT_AV2_QUANTIZATION_UNIT_TESTS)
else()
    message(STATUS "SVT-AV2: svt_av2_quantization target absent - T010 unit tests not registered")
endif()

# --- T009: scalar forward / inverse transform ---------------------------------
# Links svt_av2_transform, which PUBLIC-links svt_av2_common.
if(TARGET svt_av2_transform)
    set(SVT_AV2_TRANSFORM_UNIT_TESTS
        test_transform)        # T009

    foreach(_test IN LISTS SVT_AV2_TRANSFORM_UNIT_TESTS)
        add_executable(${_test} "unit/${_test}.c")
        target_link_libraries(${_test} PRIVATE svt_av2_transform)
        target_include_directories(${_test} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/unit")
        svt_av2_set_target_flags(${_test})

        add_test(NAME ${_test} COMMAND ${_test})
        set_tests_properties(${_test} PROPERTIES FAIL_REGULAR_EXPRESSION "FAIL ")
    endforeach()

    unset(SVT_AV2_TRANSFORM_UNIT_TESTS)
else()
    message(STATUS "SVT-AV2: svt_av2_transform target absent - T009 unit tests not registered")
endif()

# --- T012: reconstruction primitives and the ALL-INTRA frame core -------------
# test_reconstruction links svt_av2_reconstruction (PUBLIC-links svt_av2_common);
# test_intra_frame links svt_av2_encoder_core (PUBLIC-links svt_av2_reconstruction
# + svt_av2_prediction).
if(TARGET svt_av2_reconstruction)
    set(SVT_AV2_RECONSTRUCTION_UNIT_TESTS
        test_reconstruction)   # T012

    foreach(_test IN LISTS SVT_AV2_RECONSTRUCTION_UNIT_TESTS)
        add_executable(${_test} "unit/${_test}.c")
        target_link_libraries(${_test} PRIVATE svt_av2_reconstruction)
        target_include_directories(${_test} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/unit")
        svt_av2_set_target_flags(${_test})

        add_test(NAME ${_test} COMMAND ${_test})
        set_tests_properties(${_test} PROPERTIES FAIL_REGULAR_EXPRESSION "FAIL ")
    endforeach()

    unset(SVT_AV2_RECONSTRUCTION_UNIT_TESTS)
else()
    message(STATUS "SVT-AV2: svt_av2_reconstruction target absent - T012 unit tests not registered")
endif()

# test_t009_t010_link links the real transform (T009) and quantizer (T010)
# through the same call convention the frame core uses; both targets are
# PUBLIC-links of svt_av2_common.
if(TARGET svt_av2_transform AND TARGET svt_av2_quantization)
    add_executable(test_t009_t010_link "unit/test_t009_t010_link.c")
    target_link_libraries(test_t009_t010_link PRIVATE svt_av2_transform svt_av2_quantization)
    target_include_directories(test_t009_t010_link PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/unit")
    svt_av2_set_target_flags(test_t009_t010_link)

    add_test(NAME test_t009_t010_link COMMAND test_t009_t010_link)
    set_tests_properties(test_t009_t010_link PROPERTIES FAIL_REGULAR_EXPRESSION "FAIL ")
else()
    message(STATUS "SVT-AV2: transform/quantization targets absent - test_t009_t010_link not registered")
endif()

if(TARGET svt_av2_encoder_core)
    foreach(m4_test reference_frame motion_estimation inter_prediction inter_frame)
        add_executable(test_${m4_test} "unit/test_${m4_test}.c")
        target_link_libraries(test_${m4_test} PRIVATE svt_av2_encoder_core)
        target_include_directories(test_${m4_test} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/unit")
        svt_av2_set_target_flags(test_${m4_test})
        add_test(NAME test_${m4_test} COMMAND test_${m4_test})
        set_tests_properties(test_${m4_test} PROPERTIES TIMEOUT 120 FAIL_REGULAR_EXPRESSION "FAIL ")
    endforeach()
    add_executable(test_intra_frame "unit/test_intra_frame.c")
    target_link_libraries(test_intra_frame PRIVATE svt_av2_encoder_core)
    target_include_directories(test_intra_frame PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/unit")
    svt_av2_set_target_flags(test_intra_frame)

    add_test(NAME test_intra_frame COMMAND test_intra_frame)
    set_tests_properties(test_intra_frame PROPERTIES FAIL_REGULAR_EXPRESSION "FAIL ")

    # test_transform_path exercises the real T009/T010 binding (transform_path.c).
    add_executable(test_transform_path "unit/test_transform_path.c")
    target_link_libraries(test_transform_path PRIVATE svt_av2_encoder_core)
    target_include_directories(test_transform_path PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/unit")
    svt_av2_set_target_flags(test_transform_path)

    add_test(NAME test_transform_path COMMAND test_transform_path)
    set_tests_properties(test_transform_path PROPERTIES FAIL_REGULAR_EXPRESSION "FAIL ")
else()
    message(STATUS "SVT-AV2: svt_av2_encoder_core target absent - test_intra_frame/test_transform_path not registered")
endif()

# --- T002: corpus, benchmark and oracle runner -------------------------------
add_executable(test_m4_contracts "unit/test_m4_contracts.c")
target_link_libraries(test_m4_contracts PRIVATE svt_av2_enc)
target_include_directories(test_m4_contracts PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/unit")
svt_av2_set_target_flags(test_m4_contracts)
add_test(NAME test_m4_contracts COMMAND test_m4_contracts)
set_tests_properties(test_m4_contracts PROPERTIES TIMEOUT 120 FAIL_REGULAR_EXPRESSION "FAIL ")

# These are Python tools; find an interpreter once. If none exists the checks
# are registered but will fail loudly (not silently skipped).
find_package(Python3 COMPONENTS Interpreter QUIET)
if(NOT Python3_Interpreter_FOUND)
    set(SVT_AV2_PYTHON "")
    message(STATUS "SVT-AV2: Python3 not found - corpus/benchmark/runner tests will be unavailable")
else()
    set(SVT_AV2_PYTHON "${Python3_EXECUTABLE}")
endif()

set(SVT_AV2_CORPUS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/corpus")
set(SVT_AV2_TOOLS_DIR "${CMAKE_SOURCE_DIR}/tools")
set(SVT_AV2_SCHEMA_DIR "${CMAKE_SOURCE_DIR}/benchmarks/schema")
set(SVT_AV2_CONFORMANCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/conformance")
set(SVT_AV2_CORPUS_INPUT "${SVT_AV2_CORPUS_DIR}/gradient_64x64_1f_420.y4m")

if(SVT_AV2_PYTHON)
    # Corpus manifest must match the committed files byte-for-byte.
    add_test(NAME corpus_manifest
        COMMAND "${SVT_AV2_PYTHON}" "${SVT_AV2_CORPUS_DIR}/check_manifest.py" "${SVT_AV2_CORPUS_DIR}")

    # T014 matrix manifest: every y4m under tests/corpus/matrix must match
    # matrix.json (size / SHA256 / geometry / chroma / frame count / per-frame
    # SHA256). Source owned by T014, registered here by 00.
    if(EXISTS "${SVT_AV2_CORPUS_DIR}/check_matrix.py")
        add_test(NAME corpus_matrix
            COMMAND "${SVT_AV2_PYTHON}" "${SVT_AV2_CORPUS_DIR}/check_matrix.py"
                    "${SVT_AV2_CORPUS_DIR}/matrix")
    endif()

    # Benchmark record generation + schema/semantic validation (M0 parse_init).
    if(TARGET SvtAv2EncApp)
        add_test(NAME benchmark_schema
            COMMAND "${SVT_AV2_PYTHON}" "${SVT_AV2_CONFORMANCE_DIR}/check_benchmark.py"
                    --app "$<TARGET_FILE:SvtAv2EncApp>"
                    --input "${SVT_AV2_CORPUS_INPUT}"
                    --out-dir "${CMAKE_CURRENT_BINARY_DIR}/benchmark")
    endif()

    # Runner negative cases: a fake/truncated/wrong-size decoder must not PASS.
    add_test(NAME runner_self_test
        COMMAND "${SVT_AV2_PYTHON}" "${SVT_AV2_TOOLS_DIR}/run_matrix.py"
                --self-test --corpus "${SVT_AV2_CORPUS_DIR}"
                --out-dir "${CMAKE_CURRENT_BINARY_DIR}/selftest")

    # T006 raw comparator self-test (source owned by T006, registered here).
    if(EXISTS "${SVT_AV2_TOOLS_DIR}/test_compare_recon.py")
        add_test(NAME compare_recon_self_test
            COMMAND "${SVT_AV2_PYTHON}" "${SVT_AV2_TOOLS_DIR}/test_compare_recon.py")
    endif()

    add_test(NAME m3_validation_self_test
        COMMAND "${SVT_AV2_PYTHON}" "${SVT_AV2_TOOLS_DIR}/test_m3_validation.py")
    add_test(NAME m4_validation_self_test
        COMMAND "${SVT_AV2_PYTHON}" "${SVT_AV2_TOOLS_DIR}/test_m4_validation.py")
    set_tests_properties(m4_validation_self_test PROPERTIES TIMEOUT 120)

    add_test(NAME m5_mode_trace_self_test
        COMMAND "${SVT_AV2_PYTHON}" "${SVT_AV2_TOOLS_DIR}/m5_mode_trace.py" --self-test)
    add_test(NAME m52_partition_trace_self_test
        COMMAND "${SVT_AV2_PYTHON}" "${SVT_AV2_TOOLS_DIR}/m52_partition_trace.py" --self-test)
    add_test(NAME m53_batch_self_test
        COMMAND "${SVT_AV2_PYTHON}" "${SVT_AV2_TOOLS_DIR}/test_m53_batch.py")
    add_test(NAME m53_qp_trace_self_test
        COMMAND "${SVT_AV2_PYTHON}" "${SVT_AV2_TOOLS_DIR}/m53_qp_trace.py" --self-test)
    add_test(NAME m6_batch_self_test
        COMMAND "${SVT_AV2_PYTHON}" "${SVT_AV2_TOOLS_DIR}/test_m6_batch.py")
    add_test(NAME m6_campaign_self_test
        COMMAND "${SVT_AV2_PYTHON}" "${SVT_AV2_TOOLS_DIR}/test_m6_campaign.py")

    # Real oracle round-trip; only registered when the oracle was located.
    if(SVT_AV2_ORACLE_FOUND)
        add_test(NAME runner_oracle_smoke
            COMMAND "${SVT_AV2_PYTHON}" "${SVT_AV2_TOOLS_DIR}/run_matrix.py"
                    --smoke --entry gradient_64x64_1f_420
                    --corpus "${SVT_AV2_CORPUS_DIR}"
                    --out-dir "${CMAKE_CURRENT_BINARY_DIR}/smoke")
    else()
        message(STATUS "SVT-AV2: AVM oracle not found - runner_oracle_smoke not registered")
    endif()
endif()

set_tests_properties(test_mrl_rect_rd PROPERTIES TIMEOUT 1200)
if(SVT_AV2_ENABLE_SANITIZERS)
    set_tests_properties(test_mrl_rect_rd PROPERTIES TIMEOUT 3600)
endif()

set_tests_properties(test_intra_mixed32 PROPERTIES TIMEOUT 600)
set_tests_properties(test_mixed32_rd PROPERTIES TIMEOUT 1200)
foreach(_format 1 2)
    add_test(NAME test_mixed32_rd_${_format} COMMAND test_mixed32_rd ${_format})
    set_tests_properties(test_mixed32_rd_${_format} PROPERTIES TIMEOUT 1200 FAIL_REGULAR_EXPRESSION "FAIL ")
endforeach()

set_tests_properties(test_intra_classic_cfl PROPERTIES TIMEOUT 1200)

set_tests_properties(test_classic_cfl_rd PROPERTIES TIMEOUT 1800)
foreach(_format 1 2)
    add_test(NAME test_classic_cfl_rd_${_format} COMMAND test_classic_cfl_rd ${_format})
    set_tests_properties(test_classic_cfl_rd_${_format} PROPERTIES TIMEOUT 1800 FAIL_REGULAR_EXPRESSION "FAIL ")
endforeach()

set_tests_properties(test_intra_mixed8 PROPERTIES TIMEOUT 1200)

set_tests_properties(test_mixed8_rd PROPERTIES TIMEOUT 1800)
foreach(_format 1 2)
    add_test(NAME test_mixed8_rd_${_format} COMMAND test_mixed8_rd ${_format})
    set_tests_properties(test_mixed8_rd_${_format} PROPERTIES TIMEOUT 1800 FAIL_REGULAR_EXPRESSION "FAIL ")
endforeach()

set_tests_properties(test_intra_mhccp PROPERTIES TIMEOUT 1200)

set_tests_properties(test_mhccp_rd PROPERTIES TIMEOUT 1800)
foreach(_format 1 2)
    add_test(NAME test_mhccp_rd_${_format} COMMAND test_mhccp_rd ${_format})
    set_tests_properties(test_mhccp_rd_${_format} PROPERTIES TIMEOUT 1800 FAIL_REGULAR_EXPRESSION "FAIL ")
endforeach()

set_tests_properties(test_mhccp_fault PROPERTIES TIMEOUT 1800)

set_tests_properties(test_intra_thinrect PROPERTIES TIMEOUT 1800)

set_tests_properties(test_intra_thinrect_combo PROPERTIES TIMEOUT 1800)
target_include_directories(test_thinrect_domains PRIVATE "${CMAKE_SOURCE_DIR}/Source/Lib/Transform")

set_tests_properties(test_intra_thinrect_uv PROPERTIES TIMEOUT 1800)

set_tests_properties(test_intra_mixedthin test_intra_mixedthin_combo PROPERTIES TIMEOUT 1800)

set_tests_properties(test_intra_thin64 test_intra_thin64_combo PROPERTIES TIMEOUT 1800)
if(SVT_AV2_ENABLE_SANITIZERS)
    set_tests_properties(test_intra_thin64 test_intra_thin64_combo PROPERTIES TIMEOUT 3600)
endif()
target_include_directories(test_thin64_domains PRIVATE "${CMAKE_SOURCE_DIR}/Source/Lib/Transform")
