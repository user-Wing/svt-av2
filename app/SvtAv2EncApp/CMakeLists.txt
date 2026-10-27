# =============================================================================
#  SvtAv2EncApp - command line front end.
# =============================================================================
add_executable(SvtAv2EncApp
    main.c)

# The app calls T005's single keyframe entry point (keyframe.h) directly, per
# the M1 decision (a); it never assembles OBUs itself. svt_av2_bitstream
# PUBLIC-links svt_av2_entropy and exposes the Bitstream include directory.
target_link_libraries(SvtAv2EncApp PRIVATE svt_av2_enc svt_av2_bitstream)

svt_av2_set_target_flags(SvtAv2EncApp)

set_target_properties(SvtAv2EncApp PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${SVT_AV2_BIN_DIR}")
