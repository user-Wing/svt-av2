# =============================================================================
#  Prediction library (T008): minimal scalar intra prediction and residual.
#
#  Depends on svt_av2_common for the picture/plane storage and the block
#  geometry it reads neighbours through (T007). Registered into the root CMake
#  by 00; this file is owned by T008.
# =============================================================================
add_library(svt_av2_prediction STATIC
    intra_prediction.c
    mhccp_prediction.c
    inter_prediction.c)

target_include_directories(svt_av2_prediction
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_SOURCE_DIR}/Source/Lib/Encoder"
    PRIVATE
        "${CMAKE_SOURCE_DIR}/include")

target_link_libraries(svt_av2_prediction PUBLIC svt_av2_common)

set_target_properties(svt_av2_prediction PROPERTIES OUTPUT_NAME "svt_av2_prediction")

svt_av2_set_target_flags(svt_av2_prediction)
svt_av2_enable_pic(svt_av2_prediction)
