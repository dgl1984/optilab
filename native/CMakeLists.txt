cmake_minimum_required(VERSION 3.20)

if (POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif()

project(optilab_core VERSION 1.5.0 LANGUAGES CXX)
include(CTest)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(OPTILAB_BUILD_JUCE_PLUGINS "Build the shared JUCE 8 VST3 and CLAP plug-ins" ON)
set(JUCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/JUCE" CACHE PATH "Path to pinned JUCE 8.0.12 source tree")
set(CLAP_JUCE_EXTENSIONS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/clap-juce-extensions" CACHE PATH "Path to pinned clap-juce-extensions source tree")
option(OPTILAB_BUILD_WINAMP "Build the 32-bit classic Winamp DSP plug-in" ON)
option(OPTILAB_BUILD_FOOBAR2000 "Build the foobar2000 DSP component" OFF)
set(FOOBAR2000_SDK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/foobar2000-sdk"
    CACHE PATH "Path to the extracted foobar2000 SDK 2026-10-01 root")

if (MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

configure_file(
    cmake/OptiLabVersion.h.in
    generated/OptiLabVersion.h
    @ONLY
)

set(OPTILAB_CORE_DIR core)
set(OPTILAB_WINAMP_DIR plugins/winamp)
set(OPTILAB_FOOBAR2000_DIR plugins/foobar2000)
set(OPTILAB_CLI_DIR tools/cli)
set(OPTILAB_JUCE_DIR plugins/juce)

set(OPTILAB_ENGINE_DIR engine)
set(OPTILAB_CORE_SOURCES
    ${OPTILAB_CORE_DIR}/OptiLabCore.cpp
    ${OPTILAB_ENGINE_DIR}/OptiLabAnalysis.cpp
    ${OPTILAB_ENGINE_DIR}/OptiLabCorePipeline.cpp
    ${OPTILAB_ENGINE_DIR}/OptiLabCoreEngine.cpp
)

add_library(optilab-core STATIC ${OPTILAB_CORE_SOURCES})
target_include_directories(optilab-core PUBLIC ${OPTILAB_CORE_DIR} ${OPTILAB_ENGINE_DIR})

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

if (OPTILAB_BUILD_JUCE_PLUGINS AND WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8)
    if (NOT EXISTS "${JUCE_DIR}/CMakeLists.txt")
        message(FATAL_ERROR "JUCE 8.0.12 was not found at ${JUCE_DIR}. Run scripts/fetch-juce-clap-deps.ps1 first.")
    endif()
    if (NOT EXISTS "${CLAP_JUCE_EXTENSIONS_DIR}/CMakeLists.txt")
        message(FATAL_ERROR "clap-juce-extensions was not found at ${CLAP_JUCE_EXTENSIONS_DIR}. Run scripts/fetch-juce-clap-deps.ps1 first.")
    endif()

    add_subdirectory("${JUCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/JUCE" EXCLUDE_FROM_ALL)
    set(CLAP_JUCE_EXTENSIONS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    add_subdirectory("${CLAP_JUCE_EXTENSIONS_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/clap-juce-extensions" EXCLUDE_FROM_ALL)

    juce_add_plugin(OptiLabCoreJuce
        COMPANY_NAME "Lanes Audio"
        BUNDLE_ID "com.lanesaudio.optilabcore"
        PLUGIN_MANUFACTURER_CODE LnAu
        PLUGIN_CODE OpCr
        FORMATS VST3
        PRODUCT_NAME "OptiLab Core"
        COMPANY_WEBSITE "https://lanesaudio.com"
        COMPANY_EMAIL "info@lanesaudio.com"
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS TRUE
        COPY_PLUGIN_AFTER_BUILD FALSE)

    target_sources(OptiLabCoreJuce PRIVATE
        ${OPTILAB_JUCE_DIR}/PluginProcessor.cpp
        ${OPTILAB_JUCE_DIR}/PluginEditor.cpp
        ${OPTILAB_JUCE_DIR}/AccessibleParameterSlider.cpp)
    target_include_directories(OptiLabCoreJuce PRIVATE
        ${OPTILAB_JUCE_DIR}
        ${OPTILAB_CORE_DIR})
    target_compile_definitions(OptiLabCoreJuce PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_REPORT_APP_USAGE=0
        JUCE_VST3_CAN_REPLACE_VST2=0)
    target_link_libraries(OptiLabCoreJuce PRIVATE
        optilab-core
        juce::juce_audio_utils
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
    if (MSVC)
        target_compile_options(OptiLabCoreJuce PRIVATE /W4 /permissive- $<$<CONFIG:Release>:/GL>)
        target_link_options(OptiLabCoreJuce PRIVATE $<$<CONFIG:Release>:/LTCG>)
    endif()

    clap_juce_extensions_plugin(
        TARGET OptiLabCoreJuce
        CLAP_USE_JUCE_PARAMETER_RANGES ALL
        CLAP_ID "com.lanesaudio.optilab-core"
        CLAP_FEATURES audio-effect compressor limiter mastering stereo)
endif()

if (OPTILAB_BUILD_FOOBAR2000)
    if (NOT MSVC OR NOT WIN32)
        message(FATAL_ERROR "The foobar2000 component requires MSVC on Windows.")
    endif()
    if (NOT EXISTS "${FOOBAR2000_SDK_ROOT}/sdk-license.txt" OR
        NOT EXISTS "${FOOBAR2000_SDK_ROOT}/foobar2000/SDK/foobar2000.h")
        message(FATAL_ERROR
            "foobar2000 SDK 2026-10-01 was not found at ${FOOBAR2000_SDK_ROOT}. "
            "Run scripts/setup-foobar2000-sdk.ps1 first or set FOOBAR2000_SDK_ROOT.")
    endif()

    add_library(optilab-core-foobar STATIC ${OPTILAB_CORE_SOURCES})
    target_include_directories(optilab-core-foobar PUBLIC ${OPTILAB_CORE_DIR} ${OPTILAB_ENGINE_DIR})
    set_property(TARGET optilab-core-foobar PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    target_compile_options(optilab-core-foobar PRIVATE /W4 /permissive- $<$<CONFIG:Release>:/GL>)
    target_link_options(optilab-core-foobar INTERFACE $<$<CONFIG:Release>:/LTCG>)
    enable_language(RC)
    file(GLOB FOOBAR2000_PFC_SOURCES CONFIGURE_DEPENDS
        "${FOOBAR2000_SDK_ROOT}/pfc/*.cpp")
    list(FILTER FOOBAR2000_PFC_SOURCES EXCLUDE REGEX
        "/(nix-objects|pfc-fb2k-hooks|synchro_nix)\\.cpp$")
    file(GLOB FOOBAR2000_API_SOURCES CONFIGURE_DEPENDS
        "${FOOBAR2000_SDK_ROOT}/foobar2000/SDK/*.cpp")

    add_library(optilab-foobar2000 MODULE
        ${OPTILAB_FOOBAR2000_DIR}/OptiLabFoobar2000.cpp
        ${OPTILAB_FOOBAR2000_DIR}/OptiLabFoobar2000.rc
        ${FOOBAR2000_PFC_SOURCES}
        ${FOOBAR2000_API_SOURCES}
        "${FOOBAR2000_SDK_ROOT}/foobar2000/foobar2000_component_client/component_client.cpp"
    )
    target_include_directories(optilab-foobar2000 PRIVATE
        ${OPTILAB_FOOBAR2000_DIR}
        ${OPTILAB_CORE_DIR}
        ${OPTILAB_ENGINE_DIR}
        "${FOOBAR2000_SDK_ROOT}"
        "${FOOBAR2000_SDK_ROOT}/foobar2000"
        "${CMAKE_CURRENT_BINARY_DIR}/generated"
    )
    target_compile_definitions(optilab-foobar2000 PRIVATE
        UNICODE
        _UNICODE
        NOMINMAX
    )
    target_compile_options(optilab-foobar2000 PRIVATE /W3 /permissive-)
    set_property(TARGET optilab-foobar2000 PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

    if (CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(FOOBAR2000_SHARED_LIBRARY
            "${FOOBAR2000_SDK_ROOT}/foobar2000/shared/shared-x64.lib")
    else()
        set(FOOBAR2000_SHARED_LIBRARY
            "${FOOBAR2000_SDK_ROOT}/foobar2000/shared/shared-Win32.lib")
    endif()
    target_link_libraries(optilab-foobar2000 PRIVATE
        optilab-core-foobar
        "${FOOBAR2000_SHARED_LIBRARY}"
        comctl32
        shlwapi
        user32
        version
        winmm
    )
    set_target_properties(optilab-foobar2000 PROPERTIES
        OUTPUT_NAME "foo_optilab_core"
        PREFIX ""
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

option(OPTILAB_REQUIRE_MAINTAINER_TESTS
    "Fail configuration if the private maintainer regression suite is unavailable" OFF)

if (BUILD_TESTING)
    # Public tests ship with the source package and must remain reproducible.
    add_executable(optilab-core-public-regression ../tests/CorePublicRegression.cpp)
    target_link_libraries(optilab-core-public-regression PRIVATE optilab-core)
    add_test(NAME core-public-regression COMMAND optilab-core-public-regression)

    add_executable(optilab-wav-file-test ../tests/WavFileTest.cpp ${OPTILAB_CLI_DIR}/WavFile.cpp)
    target_include_directories(optilab-wav-file-test PRIVATE ${OPTILAB_CLI_DIR})
    add_test(NAME wav-file-public COMMAND optilab-wav-file-test)

    add_executable(optilab-winamp-pcm-test ../tests/WinampPcmTest.cpp)
    target_link_libraries(optilab-winamp-pcm-test PRIVATE optilab-winamp-pcm)
    add_test(NAME winamp-pcm-public COMMAND optilab-winamp-pcm-test)

    add_executable(optilab-core-behavior-regression ../tests/CoreBehaviorRegression.cpp)
    target_link_libraries(optilab-core-behavior-regression PRIVATE optilab-core)
    add_test(NAME core-behavior-public COMMAND optilab-core-behavior-regression)

    add_executable(optilab-winamp-delivery-test ../tests/WinampDeliveryTest.cpp)
    target_link_libraries(optilab-winamp-delivery-test PRIVATE optilab-winamp-pcm)
    add_test(NAME winamp-delivery COMMAND optilab-winamp-delivery-test)

    if (WIN32)
        add_executable(optilab-winamp-wrapper-test ../tests/WinampWrapperTest.cpp
            ${OPTILAB_WINAMP_DIR}/OptiLabWinampDsp.rc)
        target_include_directories(optilab-winamp-wrapper-test PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
        target_compile_definitions(optilab-winamp-wrapper-test PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
        target_link_libraries(optilab-winamp-wrapper-test PRIVATE optilab-core optilab-winamp-pcm advapi32 comctl32 user32)
        add_test(NAME winamp-wrapper COMMAND optilab-winamp-wrapper-test)
    endif()

    if (TARGET OptiLabCoreJuce)
        add_executable(optilab-juce-wrapper-test ../tests/JuceWrapperTest.cpp)
        target_include_directories(optilab-juce-wrapper-test PRIVATE ${OPTILAB_JUCE_DIR}
            "$<TARGET_PROPERTY:OptiLabCoreJuce,INCLUDE_DIRECTORIES>")
        target_compile_definitions(optilab-juce-wrapper-test PRIVATE
            "$<TARGET_PROPERTY:OptiLabCoreJuce,COMPILE_DEFINITIONS>")
        target_link_libraries(optilab-juce-wrapper-test PRIVATE OptiLabCoreJuce)
        add_test(NAME juce-wrapper COMMAND optilab-juce-wrapper-test)
    endif()

    set(OPTILAB_MAINTAINER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../maintainer")
    set(OPTILAB_MAINTAINER_ENTRY "${OPTILAB_MAINTAINER_DIR}/CoreRegression.cpp")
    if (EXISTS "${OPTILAB_MAINTAINER_ENTRY}")
        message(STATUS "OptiLab maintainer regression suite enabled")
        add_executable(optilab-core-regression ${OPTILAB_MAINTAINER_ENTRY} ${OPTILAB_CLI_DIR}/WavFile.cpp)
        target_include_directories(optilab-core-regression PRIVATE ${OPTILAB_CLI_DIR})
        add_dependencies(optilab-core-regression optilab-core-cli)
        target_link_libraries(optilab-core-regression PRIVATE optilab-core)
        add_test(NAME core-regression COMMAND optilab-core-regression "${OPTILAB_MAINTAINER_DIR}/fixtures" "$<TARGET_FILE:optilab-core-cli>")

    elseif (OPTILAB_REQUIRE_MAINTAINER_TESTS)
        message(FATAL_ERROR
            "Maintainer validation was requested, but ${OPTILAB_MAINTAINER_ENTRY} is absent. "
            "Use the maintainer/review tree or disable OPTILAB_REQUIRE_MAINTAINER_TESTS.")
    else()
        message(STATUS "Maintainer regression suite not present; public tests remain enabled")
    endif()
endif()
