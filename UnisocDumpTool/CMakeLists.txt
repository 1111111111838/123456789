cmake_minimum_required(VERSION 3.15)
project(UnisocDumpTool C)

#
# Unisoc Mocor RTOS Flash Dump Tool
#
# Targets:
#   UnisocDumpTool  - the Win32 GUI application (Windows only)
#   udt_tests       - portable core tests (all platforms)
#
# Visual Studio:
#   cmake -S . -B build -G "Visual Studio 17 2022" -A x64
#   cmake --build build --config Release
#   (open build/UnisocDumpTool.sln to work in the IDE)
#
# MinGW / command line:
#   cmake -S . -B build -G "MinGW Makefiles"
#   cmake --build build
#

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/W4 /utf-8)
    # Unicode throughout (wWinMain, *W APIs).
    add_compile_definitions(UNICODE _UNICODE _CRT_SECURE_NO_WARNINGS)
    add_compile_definitions(_WIN32_WINNT=0x0601)
else()
    add_compile_options(-Wall -Wextra)
endif()

# ---- Portable engine sources (no UI, no Win32 GUI) ---------------------------
set(CORE_SOURCES
    src/common/sha256.c
    src/common/crc16.c
    src/common/logging.c
    src/common/appconfig.c
    src/comm/transport.c
    src/protocol/hdlc.c
    src/protocol/sprd_proto.c
    src/protocol/fdl.c
    src/detect/chipdb.c
    src/detect/device_detect.c
    src/sim/sim_device.c
    src/fileout/dump_writer.c
    src/flash/flash_dump.c
)

# Platform-specific transport (real serial is Windows-only; the file itself
# compiles to a stub elsewhere).
set(PLATFORM_SOURCES
    src/comm/serial.c
)

# ---- Tests (portable) --------------------------------------------------------
add_executable(udt_tests
    tests/test_runner.c
    ${CORE_SOURCES}
    ${PLATFORM_SOURCES}
)
enable_testing()
add_test(NAME core_tests COMMAND udt_tests)

# ---- GUI application (Windows only) -----------------------------------------
if(WIN32)
    enable_language(RC)
    add_executable(UnisocDumpTool WIN32
        src/main.c
        src/ui/ui_main.c
        src/ui/ui_theme.c
        ${CORE_SOURCES}
        ${PLATFORM_SOURCES}
        src/ui/app.rc
    )
    target_include_directories(UnisocDumpTool PRIVATE src/ui)
    target_link_libraries(UnisocDumpTool PRIVATE
        comctl32 setupapi shell32 ole32 uuid comdlg32
    )
    # Place the sample INI next to the built exe for convenience.
    add_custom_command(TARGET UnisocDumpTool POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_SOURCE_DIR}/UnisocDumpTool.sample.ini
            $<TARGET_FILE_DIR:UnisocDumpTool>/UnisocDumpTool.sample.ini)
else()
    message(STATUS "Non-Windows build: GUI target skipped; building udt_tests only.")
endif()
