# =============================================================================
#  Bitstream library (T003): bit/byte writing, LEB128 and OBU framing.
#  Also carries sequence_header (T004) and frame_header (T005), both of which
#  reuse the T003 writer.
#
#  Registered into the build by the file owner (T001 root CMakeLists.txt adds
#  this subdirectory). Owned by T003; sequence_header.c (T004) and
#  frame_header.c (T005) added by 00 during M1 integration so the module builds
#  as a single target.
# =============================================================================
add_library(svt_av2_bitstream STATIC
    bit_writer.c
    obu_writer.c
    sequence_header.c
    frame_header.c
    tile_writer.c
    keyframe.c
    ccso64.c)

target_include_directories(svt_av2_bitstream
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}")

# frame_header.c and tile_writer.c (T005) call the T005 entropy encoder; block_writer.c
# lives in the svt_av2_entropy module.
target_link_libraries(svt_av2_bitstream PUBLIC svt_av2_entropy svt_av2_common svt_av2_prediction)

set_target_properties(svt_av2_bitstream PROPERTIES OUTPUT_NAME "svt_av2_bitstream")

svt_av2_set_target_flags(svt_av2_bitstream)
svt_av2_enable_pic(svt_av2_bitstream)
