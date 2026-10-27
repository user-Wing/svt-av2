# =============================================================================
#  Quantization library (T010): fixed-QP forward quantizer and the decoder-path
#  dequantizer.
#
#  Depends on svt_av2_common for the shared sample/coefficient types it includes
#  through common.h. Registered into the root CMake by 00; this file is owned by
#  00 (module CMake files are shared registration, per README section 4).
# =============================================================================
add_library(svt_av2_quantization STATIC
    quantization.c)

target_include_directories(svt_av2_quantization
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
    PRIVATE
        "${CMAKE_SOURCE_DIR}/include")

target_link_libraries(svt_av2_quantization PUBLIC svt_av2_common)

set_target_properties(svt_av2_quantization PROPERTIES OUTPUT_NAME "svt_av2_quantization")

svt_av2_set_target_flags(svt_av2_quantization)
svt_av2_enable_pic(svt_av2_quantization)

if(CMAKE_SIZEOF_VOID_P EQUAL 8 AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(AMD64|amd64|x86_64|x64)$"
   AND (MSVC OR CMAKE_C_COMPILER_ID STREQUAL "GNU"))
    add_library(svt_av2_quant_avx2 OBJECT quantization_avx2.c)
    target_link_libraries(svt_av2_quant_avx2 PRIVATE svt_av2_common)
    target_include_directories(svt_av2_quant_avx2 PRIVATE "${CMAKE_SOURCE_DIR}/include")
    target_compile_definitions(svt_av2_quantization PRIVATE SVT_AV2_HAVE_QUANT_AVX2=1)
    target_compile_definitions(svt_av2_quant_avx2 PRIVATE SVT_AV2_HAVE_QUANT_AVX2=1)
    svt_av2_set_target_flags(svt_av2_quant_avx2)
    svt_av2_enable_pic(svt_av2_quant_avx2)
    set_target_properties(svt_av2_quant_avx2 PROPERTIES INTERPROCEDURAL_OPTIMIZATION FALSE)
    if(MSVC)
        target_compile_options(svt_av2_quant_avx2 PRIVATE /arch:AVX2 /GL-)
    else()
        target_compile_options(svt_av2_quant_avx2 PRIVATE -mavx2 -fno-lto)
    endif()
    target_sources(svt_av2_quantization PRIVATE $<TARGET_OBJECTS:svt_av2_quant_avx2>)
endif()
