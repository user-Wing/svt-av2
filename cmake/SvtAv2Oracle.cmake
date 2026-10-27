# =============================================================================
#  Locate the AVM reference tools (oracle) used for verification.
#
#  The oracle is never built by this project. It is only *found* and recorded so
#  that tests and scripts can compare against a fixed, frozen reference.
#
#  Search order:
#    1. -DSVT_AV2_ORACLE_DIR=<dir>  (explicit request, honoured first)
#    2. environment variable AV2_AVMDEC (full path to avmdec)
#    3. environment variable AV2_ORACLE_BIN (directory)
#    4. environment variable AV2_PACKAGE + /tools/bin
#    5. <project>/../AVM-AV2-TEST/tools/bin
#    6. PATH
#
#  Result variables:
#    SVT_AV2_ORACLE_FOUND   - TRUE / FALSE
#    SVT_AV2_ORACLE_STATUS  - human readable status string
#    SVT_AV2_ORACLE_DIR     - directory of the resolved oracle
#    SVT_AV2_AVMDEC         - full path to the reference decoder
#    SVT_AV2_AVMENC         - full path to the reference encoder
# =============================================================================
include_guard(GLOBAL)

# Preserve an explicit -D SVT_AV2_ORACLE_DIR before the defaults reset it: the
# candidate collection below reads SVT_AV2_ORACLE_DIR, so an unconditional reset
# would silently discard a user-supplied directory (contract: the path CMake
# records must match the path the runner uses). Captured first, offered first.
set(_svt_av2_user_oracle_dir "${SVT_AV2_ORACLE_DIR}")

set(SVT_AV2_ORACLE_FOUND FALSE)
set(SVT_AV2_ORACLE_STATUS "not found")
set(SVT_AV2_ORACLE_DIR "")
set(SVT_AV2_AVMDEC "")
set(SVT_AV2_AVMENC "")

if(WIN32)
    set(_svt_av2_dec_names avmdec.exe avmdec)
    set(_svt_av2_enc_names avmenc.exe avmenc)
else()
    set(_svt_av2_dec_names avmdec)
    set(_svt_av2_enc_names avmenc)
endif()

set(_svt_av2_oracle_candidates "")

if(_svt_av2_user_oracle_dir AND EXISTS "${_svt_av2_user_oracle_dir}")
    list(APPEND _svt_av2_oracle_candidates "${_svt_av2_user_oracle_dir}")
endif()
if(SVT_AV2_ORACLE_DIR)
    list(APPEND _svt_av2_oracle_candidates "${SVT_AV2_ORACLE_DIR}")
endif()
if(DEFINED ENV{AV2_AVMDEC} AND NOT "$ENV{AV2_AVMDEC}" STREQUAL "")
    get_filename_component(_svt_av2_env_dec_dir "$ENV{AV2_AVMDEC}" DIRECTORY)
    list(APPEND _svt_av2_oracle_candidates "${_svt_av2_env_dec_dir}")
endif()
if(DEFINED ENV{AV2_ORACLE_BIN} AND NOT "$ENV{AV2_ORACLE_BIN}" STREQUAL "")
    list(APPEND _svt_av2_oracle_candidates "$ENV{AV2_ORACLE_BIN}")
endif()
if(DEFINED ENV{AV2_PACKAGE} AND NOT "$ENV{AV2_PACKAGE}" STREQUAL "")
    list(APPEND _svt_av2_oracle_candidates "$ENV{AV2_PACKAGE}/tools/bin")
endif()
list(APPEND _svt_av2_oracle_candidates
     "${CMAKE_CURRENT_SOURCE_DIR}/../AVM-AV2-TEST/tools/bin"
     "${CMAKE_CURRENT_SOURCE_DIR}/../AVM-AV2-TEST/bin")

foreach(_candidate IN LISTS _svt_av2_oracle_candidates)
    if(SVT_AV2_ORACLE_FOUND)
        break()
    endif()
    if(EXISTS "${_candidate}")
        foreach(_name IN LISTS _svt_av2_dec_names)
            if(EXISTS "${_candidate}/${_name}")
                set(SVT_AV2_AVMDEC "${_candidate}/${_name}")
                break()
            endif()
        endforeach()
        if(SVT_AV2_AVMDEC)
            foreach(_name IN LISTS _svt_av2_enc_names)
                if(EXISTS "${_candidate}/${_name}")
                    set(SVT_AV2_AVMENC "${_candidate}/${_name}")
                    break()
                endif()
            endforeach()
            set(SVT_AV2_ORACLE_FOUND TRUE)
            set(SVT_AV2_ORACLE_DIR "${_candidate}")
        endif()
    endif()
endforeach()

if(NOT SVT_AV2_ORACLE_FOUND)
    find_program(SVT_AV2_AVMDEC NAMES ${_svt_av2_dec_names})
    if(SVT_AV2_AVMDEC)
        get_filename_component(SVT_AV2_ORACLE_DIR "${SVT_AV2_AVMDEC}" DIRECTORY)
        find_program(SVT_AV2_AVMENC NAMES ${_svt_av2_enc_names})
        set(SVT_AV2_ORACLE_FOUND TRUE)
    endif()
endif()

if(SVT_AV2_ORACLE_FOUND)
    if(SVT_AV2_AVMENC)
        set(SVT_AV2_ORACLE_STATUS "found at ${SVT_AV2_ORACLE_DIR}")
    else()
        set(SVT_AV2_ORACLE_STATUS "found at ${SVT_AV2_ORACLE_DIR} (encoder missing)")
    endif()
else()
    set(SVT_AV2_ORACLE_STATUS "not found - oracle-dependent tests will be skipped")
    message(STATUS "SVT-AV2: AVM oracle not found, oracle-dependent tests will be skipped")
endif()

unset(_svt_av2_dec_names)
unset(_svt_av2_enc_names)
unset(_svt_av2_oracle_candidates)
unset(_svt_av2_user_oracle_dir)
unset(_svt_av2_env_dec_dir)
unset(_candidate)
unset(_name)
