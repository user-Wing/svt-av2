# =============================================================================
#  Encoder frame core (T012): ALL-INTRA per-frame chain driver.
#
#  Depends on svt_av2_common (picture/geometry, T007), svt_av2_prediction
#  (intra prediction + residual, T008) and svt_av2_reconstruction (T012's own
#  clipped write / scratch / export). The transform/quantisation step is a
#  function-pointer seam (intra_frame.h) and stays free of T009/T010 headers.
#
#  transform_path.c is the ONE place that binds the real T009/T010 modules to
#  that seam, so this target does depend on them for the production path. A
#  build without T009/T010 should drop transform_path.c rather than rewire the
#  core, so the seam never leaks into intra_frame.c.
#
#  NOTE (T012): created by T012 to unblock its own build; see the note in
#  Source/Lib/Reconstruction/CMakeLists.txt.
# =============================================================================
add_library(svt_av2_encoder_core STATIC
    intra_frame.c
    reference_frame.c
    motion_estimation.c
    inter_frame.c
    transform_path.c
    rd_frame.c
    deblock_luma.c
    content_filter_guard.c)

target_include_directories(svt_av2_encoder_core
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
    PRIVATE
        "${CMAKE_SOURCE_DIR}/include")

target_link_libraries(svt_av2_encoder_core
    PUBLIC
        svt_av2_reconstruction
        svt_av2_prediction
        svt_av2_bitstream
    PRIVATE
        svt_av2_transform
        svt_av2_quantization)

set_target_properties(svt_av2_encoder_core PROPERTIES OUTPUT_NAME "svt_av2_encoder_core")

svt_av2_set_target_flags(svt_av2_encoder_core)
svt_av2_enable_pic(svt_av2_encoder_core)
