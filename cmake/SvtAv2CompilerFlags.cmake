# =============================================================================
#  Shared compiler / warning configuration for every SVT-AV2 target.
# =============================================================================
include_guard(GLOBAL)

function(svt_av2_set_target_flags target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /utf-8)
        if(SVT_AV2_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wshadow
            -Wpointer-arith
            -Wstrict-prototypes
            -Wmissing-prototypes
            -Wcast-align
            -Wwrite-strings)
        if(SVT_AV2_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()

    if(SVT_AV2_ENABLE_SANITIZERS)
        if(MSVC)
            message(WARNING "SVT_AV2_ENABLE_SANITIZERS: MSVC does not support this switch, ignoring")
        else()
            target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
            target_link_options(${target} PRIVATE -fsanitize=address,undefined)
        endif()
    endif()
endfunction()

# Position independent code for shared library variants.
function(svt_av2_enable_pic target)
    set_target_properties(${target} PROPERTIES POSITION_INDEPENDENT_CODE ON)
endfunction()
