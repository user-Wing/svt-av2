# =============================================================================
#  Transform library (T009): scalar forward / inverse integer transforms
#  (DCT_DCT, 8-bit, sizes <= 16x16).
#
#  Depends on svt_av2_common for common.h (shared sample / coefficient types and
#  the public header include path). The root CMake add_subdirectory() and the
#  tests/CMakeLists.txt registration are owned by 00: this file is authored by
#  T009 per docs/m1-m2-paths.md section 2.1, and 00 wires it in.
#
#  Suggested registration (00):
#    Source/Lib/Transform -> the library target svt_av2_transform below
#    tests/CMakeLists.txt -> add test_transform, linking svt_av2_transform
# =============================================================================
add_library(svt_av2_transform STATIC
    transform.c)

target_include_directories(svt_av2_transform
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
    PRIVATE
        "${CMAKE_SOURCE_DIR}/include")

target_link_libraries(svt_av2_transform PUBLIC svt_av2_common)

set_target_properties(svt_av2_transform PROPERTIES OUTPUT_NAME "svt_av2_transform")

svt_av2_set_target_flags(svt_av2_transform)
svt_av2_enable_pic(svt_av2_transform)
