# MpqFileLister - MPQDraft Plugin
# This plugin logs all SFileOpenFileEx calls to a file

cmake_minimum_required(VERSION 3.15)
project(MpqFileLister VERSION 1.0 LANGUAGES CXX)

# This is a Windows-only plugin
if(NOT WIN32)
    message(FATAL_ERROR "MpqFileLister can only be built for Windows")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Source files
set(SOURCES
    MpqFileLister.cpp
    Config.cpp
    ConfigDialog.cpp
    QHookAPI.cpp
)

set(HEADERS
    MpqFileLister.h
    Config.h
    ConfigDialog.h
    MPQDraftPlugin.h
    QHookAPI.h
)

# Create the DLL
add_library(MpqFileLister SHARED
    ${SOURCES}
    ${HEADERS}
    MpqFileLister.def
)

# Set output name to .qdp (MPQDraft plugin extension)
set_target_properties(MpqFileLister PROPERTIES
    OUTPUT_NAME "MpqFileLister"
    PREFIX ""
    SUFFIX ".qdp"
)

# Link against Windows libraries
target_link_libraries(MpqFileLister PRIVATE
    kernel32
    user32
    comdlg32
    shell32
)

# Compiler definitions
target_compile_definitions(MpqFileLister PRIVATE
    WIN32
    _WINDOWS
    _USRDLL
    MPQFILELISTER_EXPORTS
)

# For MSVC, disable some warnings and enable SEH
if(MSVC)
    target_compile_options(MpqFileLister PRIVATE
        /W3
        /EHa  # Enable SEH exceptions
    )
endif()

# For MinGW, we need to handle SEH differently and link statically
if(MINGW)
    target_compile_options(MpqFileLister PRIVATE
        -Wall
        -Wextra
    )
    # Link statically to avoid runtime dependencies on libstdc++ and libgcc
    target_link_options(MpqFileLister PRIVATE
        -static-libgcc
        -static-libstdc++
        -static
    )
endif()
