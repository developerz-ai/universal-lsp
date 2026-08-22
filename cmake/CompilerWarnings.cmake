# One warning set, applied through a single INTERFACE target. A subsystem that
# wants different warnings is a subsystem that stops being checked — link
# `ulsp_warnings` everywhere instead.
add_library(ulsp_warnings INTERFACE)

# Applies to C and C++ alike. graph-lite's public header is C, and the test that
# proves it is a C translation unit — handing it C++-only flags produces a wall of
# "valid for C++ but not for C" noise that trains everyone to ignore the output.
target_compile_options(ulsp_warnings INTERFACE
    -Wall
    -Wextra
    -Wpedantic
    -Wshadow
    -Wcast-align
    -Wunused
    -Wconversion
    -Wsign-conversion
    -Wdouble-promotion
    -Wformat=2
    # Position encoding, arena offsets and CSR indices are all integer maths on
    # untrusted input. -Wconversion above is the one that catches the class of
    # bug that panics a language server on an astral-plane codepoint.
    -Wnull-dereference)

# C++ only.
target_compile_options(ulsp_warnings INTERFACE
    $<$<COMPILE_LANGUAGE:CXX>:-Wnon-virtual-dtor>
    $<$<COMPILE_LANGUAGE:CXX>:-Woverloaded-virtual>
    $<$<COMPILE_LANGUAGE:CXX>:-Wold-style-cast>)

if(ULSP_WERROR)
    target_compile_options(ulsp_warnings INTERFACE -Werror)
endif()
