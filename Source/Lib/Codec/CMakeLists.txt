# =============================================================================
#  Encoder library: M0 skeleton of the public encoder API.
#
#  Two variants are produced:
#    svt_av2_enc        - static library (default link target for the app/tests)
#    svt_av2_enc_shared - optional shared library used to check the exported
#                         symbol surface early (plan section 4.2)
# =============================================================================
add_library(svt_av2_enc STATIC
    encoder.c)

# M2 (T013): the encoder context owns the internal reconstruction picture and
# the recon scratch, so codec.h includes reconstruction.h. The M2 encode chain
# drives T012's frame core, so the Codec->Encoder/Reconstruction edge is the
# M2 layering direction (ruled by 00, 2026-10-07).
#
# The context also holds a SvtAv2TransformPathState, so codec.h includes
# transform_path.h; svt_av2_encoder_core PUBLIC-exports its own include
# directory (Source/Lib/Encoder) and PUBLIC-links reconstruction/prediction, so
# the single link below supplies both the header path and the symbols. There is
# no cycle: svt_av2_encoder_core does not link svt_av2_enc.
target_include_directories(svt_av2_enc
    PUBLIC
        "${CMAKE_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_SOURCE_DIR}/Source/Lib/Common"
    PRIVATE
        "${CMAKE_SOURCE_DIR}/Source/Lib/Bitstream"
        "${CMAKE_SOURCE_DIR}/Source/Lib/Reconstruction")

target_link_libraries(svt_av2_enc PUBLIC svt_av2_common svt_av2_bitstream svt_av2_reconstruction svt_av2_encoder_core svt_av2_restoration)

set_target_properties(svt_av2_enc PROPERTIES OUTPUT_NAME "svt_av2_enc")

svt_av2_set_target_flags(svt_av2_enc)
svt_av2_enable_pic(svt_av2_enc)

if(SVT_AV2_BUILD_SHARED)
    add_library(svt_av2_enc_shared SHARED
        encoder.c)

    target_include_directories(svt_av2_enc_shared
        PUBLIC
            "${CMAKE_SOURCE_DIR}/include"
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}"
            "${CMAKE_SOURCE_DIR}/Source/Lib/Common"
            "${CMAKE_SOURCE_DIR}/Source/Lib/Bitstream"
            "${CMAKE_SOURCE_DIR}/Source/Lib/Reconstruction")

    target_link_libraries(svt_av2_enc_shared PUBLIC svt_av2_common svt_av2_bitstream svt_av2_reconstruction svt_av2_encoder_core svt_av2_restoration)

    target_compile_definitions(svt_av2_enc_shared PRIVATE SVT_AV2_BUILD_SHARED)

    # On Windows a SHARED target also emits an import library (.lib) with the
    # same base name as the DLL. Reusing "svt_av2_enc" for both variants made
    # the static archive and the shared import library collide (ninja: multiple
    # rules generate lib/svt_av2_enc.lib). Give the shared variant a distinct
    # output name so both can coexist in one build tree.
    set_target_properties(svt_av2_enc_shared PROPERTIES
        OUTPUT_NAME "svt_av2_enc_shared"
        C_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON)

    svt_av2_set_target_flags(svt_av2_enc_shared)
    svt_av2_enable_pic(svt_av2_enc_shared)
endif()
