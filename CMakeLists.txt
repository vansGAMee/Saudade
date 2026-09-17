cmake_minimum_required(VERSION 3.25)
project(Saudade
    VERSION 0.1.0
    LANGUAGES C CXX
    DESCRIPTION "Saudade Linux DAW - Vertical Slice Architecture Proof v0"
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(SAUDADE_ENABLE_SANITIZERS "Enable Address and Undefined Behavior Sanitizers" OFF)
option(SAUDADE_ENABLE_TSAN "Enable ThreadSanitizer" OFF)
option(SAUDADE_BUILD_TESTS "Build Saudade test suite" ON)

# Compiler warning flags for project targets
function(saudade_set_warnings target)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(${target} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wshadow
            -Wnon-virtual-dtor
            -Wold-style-cast
            -Wcast-align
            -Wunused
            -Woverloaded-virtual
        )
    endif()
endfunction()

# Sanitizers configuration
if(SAUDADE_ENABLE_SANITIZERS AND SAUDADE_ENABLE_TSAN)
    message(FATAL_ERROR "Cannot enable both SAUDADE_ENABLE_SANITIZERS and SAUDADE_ENABLE_TSAN simultaneously")
endif()

if(SAUDADE_ENABLE_SANITIZERS)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        message(STATUS "Saudade: Enabling AddressSanitizer and UndefinedBehaviorSanitizer")
        add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address,undefined)
    endif()
endif()

if(SAUDADE_ENABLE_TSAN)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        message(STATUS "Saudade: Enabling ThreadSanitizer")
        add_compile_options(-fsanitize=thread -fno-omit-frame-pointer)
        add_link_options(-fsanitize=thread)
    endif()
endif()

# Core libraries (pure C++, no PipeWire dependency)
add_subdirectory(libs/time)
add_subdirectory(libs/events)
add_subdirectory(libs/renderplan)
add_subdirectory(libs/graph)
add_subdirectory(libs/audio)

# Boundary adapter (PipeWire dependency isolated here)
find_package(PkgConfig REQUIRED)
pkg_check_modules(PIPEWIRE REQUIRED IMPORTED_TARGET libpipewire-0.3)

add_subdirectory(adapters/pipewire)

# Applications
add_subdirectory(apps/audio-proof)

# Tests
if(SAUDADE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
