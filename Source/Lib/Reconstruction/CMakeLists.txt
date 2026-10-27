# =============================================================================
#  Reconstruction library (T012): clipped reconstruction, scratch pool, export.
#
#  Depends on svt_av2_common (picture/block geometry from T007). Independent of
#  T009/T010: it consumes an inverse residual it did not compute.
#
#  NOTE (T012): this file was created by T012 to unblock its own build. The
#  paths doc says module CMakeLists belong to 00; T012 has reported it and will
#  not rename targets or remove sources without 00's agreement.
# =============================================================================
add_library(svt_av2_reconstruction STATIC
    reconstruction.c)

target_include_directories(svt_av2_reconstruction
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
    PRIVATE
        "${CMAKE_SOURCE_DIR}/include")

target_link_libraries(svt_av2_reconstruction PUBLIC svt_av2_common)

set_target_properties(svt_av2_reconstruction PROPERTIES OUTPUT_NAME "svt_av2_reconstruction")

svt_av2_set_target_flags(svt_av2_reconstruction)
svt_av2_enable_pic(svt_av2_reconstruction)
