cmake_minimum_required(VERSION 3.20)

if (POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif()

project(optilab_core VERSION 1.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(OPTILAB_BUILD_CLAP "Build the 64-bit Windows CLAP plug-in" ON)
option(OPTILAB_BUILD_WINAMP "Build the 32-bit classic Winamp DSP plug-in" ON)

if (MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

configure_file(
    cmake/OptiLabVersion.h.in
    generated/OptiLabVersion.h
    @ONLY
)

set(OPTILAB_CORE_DIR core)
set(OPTILAB_CLAP_DIR plugins/clap)
set(OPTILAB_WINAMP_DIR plugins/winamp)
set(OPTILAB_CLI_DIR tools/cli)

add_library(optilab-core STATIC
    ${OPTILAB_CORE_DIR}/OptiLabCore.cpp
)

target_include_directories(optilab-core PUBLIC ${OPTILAB_CORE_DIR})

add_executable(optilab-core-cli
    ${OPTILAB_CLI_DIR}/main.cpp
    ${OPTILAB_CLI_DIR}/WavFile.cpp
)

target_include_directories(optilab-core-cli PRIVATE ${OPTILAB_CLI_DIR})
target_link_libraries(optilab-core-cli PRIVATE optilab-core)

add_library(optilab-winamp-pcm STATIC
    ${OPTILAB_WINAMP_DIR}/WinampPcm.cpp
)
target_include_directories(optilab-winamp-pcm PUBLIC ${OPTILAB_WINAMP_DIR})

if (OPTILAB_BUILD_CLAP AND WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8)
    add_library(optilab-clap MODULE
        ${OPTILAB_CLAP_DIR}/OptiLabClap.cpp
        ${OPTILAB_CLAP_DIR}/OptiLabEditor.cpp
    )
    target_include_directories(optilab-clap PRIVATE
        ${OPTILAB_CLAP_DIR}
        ${OPTILAB_CORE_DIR}
        third_party/clap/include
        "${CMAKE_CURRENT_BINARY_DIR}/generated"
    )
    target_compile_definitions(optilab-clap PRIVATE
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
    target_link_libraries(optilab-clap PRIVATE
        optilab-core
        comctl32
        gdi32
        user32
    )
    set_target_properties(optilab-clap PROPERTIES
        OUTPUT_NAME "OptiLab_Core"
        PREFIX ""
        SUFFIX ".clap"
    )
endif()

if (MSVC)
    target_compile_options(optilab-core PRIVATE
        /W4
        /permissive-
        $<$<CONFIG:Release>:/GL>
    )
    target_link_options(optilab-core INTERFACE
        $<$<CONFIG:Release>:/LTCG>
    )
    target_compile_options(optilab-core-cli PRIVATE /W4 /permissive-)
    target_compile_options(optilab-winamp-pcm PRIVATE /W4 /permissive-)
    if (TARGET optilab-clap)
        target_compile_options(optilab-clap PRIVATE
            /W4
            /permissive-
            $<$<CONFIG:Release>:/GL>
        )
        target_link_options(optilab-clap PRIVATE
            $<$<CONFIG:Release>:/LTCG>
        )
    endif()
else()
    target_compile_options(optilab-core PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(optilab-core-cli PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(optilab-winamp-pcm PRIVATE -Wall -Wextra -Wpedantic)
endif()

if (OPTILAB_BUILD_WINAMP AND MSVC AND CMAKE_SIZEOF_VOID_P EQUAL 4)
    enable_language(RC)
    add_library(optilab-winamp-dsp MODULE
        ${OPTILAB_WINAMP_DIR}/OptiLabWinampDsp.cpp
        ${OPTILAB_WINAMP_DIR}/OptiLabWinampDsp.def
        ${OPTILAB_WINAMP_DIR}/OptiLabWinampDsp.rc
    )
    target_include_directories(optilab-winamp-dsp PRIVATE
        ${OPTILAB_WINAMP_DIR}
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

    if (TARGET optilab-clap)
        add_executable(optilab-clap-smoke-tests
            tests/ClapPluginSmokeTests.cpp
        )
        target_include_directories(optilab-clap-smoke-tests PRIVATE
            third_party/clap/include
            "${CMAKE_CURRENT_BINARY_DIR}/generated"
        )
        target_compile_definitions(optilab-clap-smoke-tests PRIVATE
            UNICODE
            _UNICODE
            WIN32_LEAN_AND_MEAN
            NOMINMAX
        )
        if (MSVC)
            target_compile_options(optilab-clap-smoke-tests PRIVATE /W4 /permissive-)
        endif()
        target_link_libraries(optilab-clap-smoke-tests PRIVATE oleacc user32)
        add_test(
            NAME optilab-clap-plugin-smoke
            COMMAND optilab-clap-smoke-tests $<TARGET_FILE:optilab-clap>
        )
    endif()

    if (TARGET optilab-winamp-dsp)
        add_executable(optilab-winamp-smoke-tests
            tests/WinampPluginSmokeTests.cpp
        )
        target_include_directories(optilab-winamp-smoke-tests PRIVATE
            ${OPTILAB_WINAMP_DIR}
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
