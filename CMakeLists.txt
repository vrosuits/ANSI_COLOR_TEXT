cmake_minimum_required(VERSION 3.20)
project(NppAnsiColor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# Core: platform-independent ANSI parsing + styling logic (no Win32/Scintilla).
# Buildable and testable on any host.
# ---------------------------------------------------------------------------
add_library(ansicore STATIC
    src/AnsiPalette.cpp
    src/AnsiSgr.cpp
    src/AnsiParser.cpp
    src/AnsiScreen.cpp
    src/AnsiStyler.cpp
)
target_include_directories(ansicore PUBLIC src)

# ---------------------------------------------------------------------------
# Host self-test for the core (no Notepad++ required).
#   cmake -B build && cmake --build build && ctest --test-dir build
# ---------------------------------------------------------------------------
option(ANSI_BUILD_TESTS "Build the host parser self-test" ON)
if(ANSI_BUILD_TESTS)
    enable_testing()
    add_executable(ansicore_test test/test_core.cpp)
    target_link_libraries(ansicore_test PRIVATE ansicore)
    add_test(NAME ansicore_test COMMAND ansicore_test)
endif()

# ---------------------------------------------------------------------------
# The Notepad++ plugin DLL (Windows only).
# Needs the Notepad++ plugin SDK headers (PluginInterface.h, Scintilla.h, ...).
# Either point -DNPP_SDK_INCLUDE=<path> at them, or let CMake fetch the official
# plugin template that bundles them.
# ---------------------------------------------------------------------------
if(WIN32)
    set(NPP_SDK_INCLUDE "" CACHE PATH "Path to Notepad++ plugin SDK headers")
    if(NOT NPP_SDK_INCLUDE)
        include(FetchContent)
        FetchContent_Declare(npp_template
            GIT_REPOSITORY https://github.com/npp-plugins/plugintemplate.git
            GIT_TAG        master)
        FetchContent_MakeAvailable(npp_template)
        set(NPP_SDK_INCLUDE ${npp_template_SOURCE_DIR}/src)
    endif()

    enable_language(RC)
    add_library(NppAnsiColor SHARED
        src/DllMain.cpp
        src/PluginDefinition.cpp
        src/Settings.cpp
        src/SettingsDialog.cpp
        src/SettingsDialog.rc
    )
    target_link_libraries(NppAnsiColor PRIVATE ansicore)
    target_include_directories(NppAnsiColor PRIVATE ${NPP_SDK_INCLUDE} src)
    target_compile_definitions(NppAnsiColor PRIVATE UNICODE _UNICODE)

    # Notepad++ loads "<name>.dll" from its plugins/<name>/ folder.
    set_target_properties(NppAnsiColor PROPERTIES
        OUTPUT_NAME "ANSI_COLOR_TEXT"
        PREFIX "")
endif()
