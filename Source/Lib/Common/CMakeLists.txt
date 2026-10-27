# =============================================================================
#  Common library: build information, geometry helpers, container parsing.
# =============================================================================
add_library(svt_av2_common STATIC
    common.c
    y4m.c
    picture.c
    block_geometry.c)

target_include_directories(svt_av2_common
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
        # common.h includes the public header svt_av2.h, so every consumer of
        # this module needs the include/ directory on its path. This must be
        # PUBLIC: T007/T008/T010 unit tests link this target directly (not
        # through svt_av2_enc) and would otherwise fail on "svt_av2.h: No such
        # file or directory".
        "${CMAKE_SOURCE_DIR}/include")

set_target_properties(svt_av2_common PROPERTIES OUTPUT_NAME "svt_av2_common")

# Build information is injected so that runtime output can state exactly which
# configuration produced a result (plan section 3.3).
target_compile_definitions(svt_av2_common PRIVATE
    SVT_AV2_BUILD_TYPE_STRING="${CMAKE_BUILD_TYPE}"
    SVT_AV2_COMPILER_STRING="${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}"
    SVT_AV2_ORACLE_DIR_STRING="${SVT_AV2_ORACLE_DIR}")

svt_av2_set_target_flags(svt_av2_common)
svt_av2_enable_pic(svt_av2_common)
