# ASan/UBSan are wired in from the first commit, not added after the first
# mystery crash: the hot paths here are arena/pmr allocation and a CSR adjacency
# structure indexed by raw offsets, where a use-after-free reads as "wrong
# answer" rather than a crash.
add_library(ulsp_sanitizers INTERFACE)

if(NOT ULSP_SANITIZE STREQUAL "OFF")
    target_compile_options(ulsp_sanitizers INTERFACE
        -fsanitize=${ULSP_SANITIZE}
        -fno-omit-frame-pointer
        -fno-sanitize-recover=all)
    target_link_options(ulsp_sanitizers INTERFACE -fsanitize=${ULSP_SANITIZE})
    message(STATUS "Sanitizers: ${ULSP_SANITIZE}")
endif()
