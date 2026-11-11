# ── UNDOER ────────────────────────────────────────────────────────────────────────────────────────────────────────────
# C++23: one undo core for every document type. A shared stack of named steps that span documents of any format, each document's
# part in it, and two ready-made ways to capture a change (per item, whole document). Standard library only.
#
# Consumed via FetchContent; consumers link `undoer::undoer` and `#include <undoer/Undoer.h>`. Layout:
#   API/undoer/  — public headers
#   src/         — implementations
#   tests/       — CTest suite
#
# The MSVC runtime is NOT set here: it inherits the consumer's.

cmake_minimum_required(VERSION 3.28)

project(UNDOER
    VERSION 0.1.0
    DESCRIPTION "One undo core for every document type: a shared step stack, per-item and snapshot document histories"
    LANGUAGES CXX)

if(PROJECT_IS_TOP_LEVEL)
    set(CMAKE_CXX_STANDARD 23)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
endif()

option(UNDOER_BUILD_TESTS "Build UNDOER tests" ${PROJECT_IS_TOP_LEVEL})

file(GLOB_RECURSE UNDOER_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
file(GLOB_RECURSE UNDOER_HEADERS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/API/undoer/*.h")

add_library(undoer STATIC ${UNDOER_SOURCES} ${UNDOER_HEADERS})
add_library(undoer::undoer ALIAS undoer)
target_include_directories(undoer PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/API>)
target_compile_features(undoer PUBLIC cxx_std_23)
if(MSVC)
    target_compile_options(undoer PRIVATE /W4 /utf-8)
endif()
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${UNDOER_SOURCES} ${UNDOER_HEADERS})

if(UNDOER_BUILD_TESTS)
    enable_testing()
    add_executable(undoer-test tests/undoer_test.cpp)
    target_link_libraries(undoer-test PRIVATE undoer::undoer)
    if(MSVC)
        target_compile_options(undoer-test PRIVATE /W4 /utf-8)
    endif()
    add_test(NAME undoer-test COMMAND undoer-test)
endif()
