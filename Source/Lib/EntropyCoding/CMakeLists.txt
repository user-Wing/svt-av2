# =============================================================================
#  Entropy coding library (T005): AV2 symbol/range encoder for tile payloads.
#
#  Deliberately separate from the T003 raw BitWriter (a raw bit writer is not
#  an entropy coder). Created and registered by 00 during M1 integration; the
#  module target svt_av2_entropy must exist before the encoder links it.
# =============================================================================
add_library(svt_av2_entropy STATIC
    entropy_encoder.c
    entropy_models.c
    entropy_model_data.c
    cdf_tables.c
    block_writer.c
    motion_syntax.c
    # T011 coefficient syntax (M2). coeff_syntax.c depends only on the local
    # headers (entropy_encoder.h + the CDF tables); it must NOT link
    # svt_av2_bitstream, which PUBLIC-links this target.
    coeff_syntax.c
    cdf_tables_coeff.c)

target_include_directories(svt_av2_entropy
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(svt_av2_entropy PUBLIC svt_av2_common)

set_target_properties(svt_av2_entropy PROPERTIES OUTPUT_NAME "svt_av2_entropy")

svt_av2_set_target_flags(svt_av2_entropy)
svt_av2_enable_pic(svt_av2_entropy)
