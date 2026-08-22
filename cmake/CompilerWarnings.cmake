# One warning set, applied through a single INTERFACE target. A subsystem that
# wants different warnings is a subsystem that stops being checked — link
# `ulsp_warnings` everywhere instead.
add_library(ulsp_warnings INTERFACE)

target_compile_options(ulsp_warnings INTERFACE
    -Wall
    -Wextra
    -Wpedantic
    -Wshadow
    -Wnon-virtual-dtor
    -Wcast-align
    -Wunused
    -Woverloaded-virtual
    -Wconversion
    -Wsign-conversion
    -Wdouble-promotion
    -Wformat=2
    # Position encoding, arena offsets and CSR indices are all integer maths on
    # untrusted input. -Wconversion above is the one that catches the class of
    # bug that panics a language server on an astral-plane codepoint.
    -Wnull-dereference
    -Wold-style-cast)

if(ULSP_WERROR)
    target_compile_options(ulsp_warnings INTERFACE -Werror)
endif()
