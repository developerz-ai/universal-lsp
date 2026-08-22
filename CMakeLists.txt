# Universal LSP + graph-lite — root build.
#
# Layout mirrors the dependency direction, which is one-way and enforced:
#   graph-lite/  embeddable graph library — depends on NOTHING in this repo
#   engine/      LSP daemon core         — depends on graph-lite
#   analysis/    LSP concepts -> graph   — the only place both vocabularies meet
#   daemon/ cli/ thin entrypoints
# `scripts/lint/graph-lite-boundary.ts` fails the gate if that direction reverses.
cmake_minimum_required(VERSION 3.24)

project(universal-lsp
    VERSION 0.0.1
    DESCRIPTION "One fast, low-memory LSP + MCP engine for many languages, plus graph-lite"
    LANGUAGES CXX C)

if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
    message(FATAL_ERROR "In-source builds are not allowed. Use `bin/check` or `cmake --preset dev`.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# clangd, clang-tidy and the editor all read compile_commands.json from the repo
# root; without this symlink every tool needs the build dir spelled out.
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

option(ULSP_BUILD_TESTS "Build the test suites" ON)
option(ULSP_WERROR      "Treat compiler warnings as errors" ON)
option(ULSP_SANITIZE    "Sanitizers to enable: OFF | address | undefined | address,undefined" "OFF")

include(CompilerWarnings)
include(Sanitizers)

add_subdirectory(graph-lite)
add_subdirectory(engine)
add_subdirectory(analysis)
add_subdirectory(daemon)
add_subdirectory(cli)

if(ULSP_BUILD_TESTS)
    include(CTest)
    include(Dependencies)
    add_subdirectory(tests)
endif()
