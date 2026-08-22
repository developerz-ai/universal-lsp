# Test-only dependencies. The shipped binaries have none beyond libstdc++ —
# "runs with no network and no other developer tools installed" is a testable
# requirement (docs/architecture/non-functional.md), so nothing here may leak
# into a non-test target.
include(FetchContent)

# Prefer a system GoogleTest so an offline box, a distro package and CI all
# work; fall back to a pinned fetch. Pin, never float: an unpinned test
# framework makes a red run ambiguous between "my change" and "their release".
find_package(GTest QUIET)

if(NOT GTest_FOUND)
    FetchContent_Declare(googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.17.0
        GIT_SHALLOW    TRUE
        EXCLUDE_FROM_ALL)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
endif()

include(GoogleTest)
