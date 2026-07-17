cmake_minimum_required(VERSION 3.20)

if (POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif()

project(optilab_core VERSION 1.1.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

configure_file(
    cmake/OptiLabVersion.h.in
    generated/OptiLabVersion.h
    @ONLY
)

add_library(optilab-core STATIC
    src/OptiLabCore.cpp
)

target_include_directories(optilab-core PUBLIC src)

add_executable(optilab-core-cli
    src/main.cpp
    src/WavFile.cpp
)

target_link_libraries(optilab-core-cli PRIVATE optilab-core)

add_library(optilab-winamp-pcm STATIC
    winamp/WinampPcm.cpp
)
target_include_directories(optilab-winamp-pcm PUBLIC winamp)

if (MSVC)
    target_compile_options(optilab-core PRIVATE /W4 /permissive-)
    target_compile_options(optilab-core-cli PRIVATE /W4 /permissive-)
    target_compile_options(optilab-winamp-pcm PRIVATE /W4 /permissive-)
else()
    target_compile_options(optilab-core PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(optilab-core-cli PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(optilab-winamp-pcm PRIVATE -Wall -Wextra -Wpedantic)
endif()

if (MSVC AND CMAKE_SIZEOF_VOID_P EQUAL 4)
    enable_language(RC)
    add_library(optilab-winamp-dsp MODULE
        winamp/OptiLabWinampDsp.cpp
        winamp/OptiLabWinampDsp.def
        winamp/OptiLabWinampDsp.rc
    )
    target_include_directories(optilab-winamp-dsp PRIVATE
        winamp
        "${CMAKE_CURRENT_BINARY_DIR}/generated"
    )
    target_compile_definitions(optilab-winamp-dsp PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
    target_compile_options(optilab-winamp-dsp PRIVATE /W4 /permissive-)
    target_link_libraries(optilab-winamp-dsp PRIVATE
        optilab-core
        optilab-winamp-pcm
        advapi32
        comctl32
        user32
        version
    )
    set_target_properties(optilab-winamp-dsp PROPERTIES
        OUTPUT_NAME "dsp_optilab_core"
        PREFIX ""
    )
endif()

include(CTest)

if (BUILD_TESTING)
    add_executable(optilab-core-tests
        tests/OptiLabCoreTests.cpp
    )
    target_link_libraries(optilab-core-tests PRIVATE optilab-core)
    if (MSVC)
        target_compile_options(optilab-core-tests PRIVATE /W4 /permissive-)
    else()
        target_compile_options(optilab-core-tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME optilab-core-api COMMAND optilab-core-tests)

    add_executable(optilab-winamp-pcm-tests
        tests/WinampPcmTests.cpp
    )
    target_link_libraries(optilab-winamp-pcm-tests PRIVATE optilab-winamp-pcm)
    if (MSVC)
        target_compile_options(optilab-winamp-pcm-tests PRIVATE /W4 /permissive-)
    else()
        target_compile_options(optilab-winamp-pcm-tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME optilab-winamp-pcm COMMAND optilab-winamp-pcm-tests)

    if (TARGET optilab-winamp-dsp)
        add_executable(optilab-winamp-smoke-tests
            tests/WinampPluginSmokeTests.cpp
        )
        target_include_directories(optilab-winamp-smoke-tests PRIVATE
            winamp
            "${CMAKE_CURRENT_BINARY_DIR}/generated"
        )
        target_compile_definitions(optilab-winamp-smoke-tests PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
        target_compile_options(optilab-winamp-smoke-tests PRIVATE /W4 /permissive-)
        target_link_libraries(optilab-winamp-smoke-tests PRIVATE version)
        add_test(
            NAME optilab-winamp-plugin-smoke
            COMMAND optilab-winamp-smoke-tests $<TARGET_FILE:optilab-winamp-dsp>
        )
    endif()
endif()
