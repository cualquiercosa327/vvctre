# CMake 3.8 required for 17 to be a valid value for CXX_STANDARD
cmake_minimum_required(VERSION 3.8)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMakeModules")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/externals/cmake-modules")
include(DownloadExternals)
include(CMakeDependentOption)

project(vvctre)

# MSVC only: bundled SDL2
CMAKE_DEPENDENT_OPTION(VVCTRE_USE_BUNDLED_SDL2 "Download bundled SDL2 binaries" ON "MSVC" OFF)

option(ENABLE_CUBEB "Enables the cubeb audio backend" ON)

option(ENABLE_FFMPEG_AUDIO_DECODER "Enable FFmpeg audio (AAC) decoder" OFF)
option(ENABLE_FFMPEG_VIDEO_DUMPER "Enable FFmpeg video dumper" OFF)

if (ENABLE_FFMPEG_AUDIO_DECODER OR ENABLE_FFMPEG_VIDEO_DUMPER)
    set(ENABLE_FFMPEG ON)
endif()

CMAKE_DEPENDENT_OPTION(VVCTRE_USE_BUNDLED_FFMPEG "Download bundled FFmpeg binaries" ON "ENABLE_FFMPEG;MSVC" OFF)

option(USE_DISCORD_PRESENCE "Enables Discord Rich Presence" OFF)

CMAKE_DEPENDENT_OPTION(ENABLE_MF "Use Media Foundation decoder (preferred over FFmpeg)" ON "WIN32" OFF)

# Sanity check : Check that all submodules are present
# =======================================================================

function(check_submodules_present)
    file(READ "${PROJECT_SOURCE_DIR}/.gitmodules" gitmodules)
    string(REGEX MATCHALL "path *= *[^ \t\r\n]*" gitmodules ${gitmodules})
    foreach(module ${gitmodules})
        string(REGEX REPLACE "path *= *" "" module ${module})
        if (NOT EXISTS "${PROJECT_SOURCE_DIR}/${module}/.git")
            message(SEND_ERROR "Git submodule ${module} not found."
                    "Please run: git submodule update --init --recursive")
        endif()
    endforeach()
endfunction()
check_submodules_present()

# Detect current compilation architecture and create standard definitions
# =======================================================================

include(CheckSymbolExists)
function(detect_architecture symbol arch)
    if (NOT DEFINED ARCHITECTURE)
        set(CMAKE_REQUIRED_QUIET 1)
        check_symbol_exists("${symbol}" "" ARCHITECTURE_${arch})
        unset(CMAKE_REQUIRED_QUIET)

        # The output variable needs to be unique across invocations otherwise
        # CMake's crazy scope rules will keep it defined
        if (ARCHITECTURE_${arch})
            set(ARCHITECTURE "${arch}" PARENT_SCOPE)
            set(ARCHITECTURE_${arch} 1 PARENT_SCOPE)
            add_definitions(-DARCHITECTURE_${arch}=1)
        endif()
    endif()
endfunction()

if (NOT ENABLE_GENERIC)
    if (MSVC)
        detect_architecture("_M_AMD64" x86_64)
        detect_architecture("_M_IX86" x86)
        detect_architecture("_M_ARM" ARM)
        detect_architecture("_M_ARM64" ARM64)
    else()
        detect_architecture("__x86_64__" x86_64)
        detect_architecture("__i386__" x86)
        detect_architecture("__arm__" ARM)
        detect_architecture("__aarch64__" ARM64)
    endif()
endif()
if (NOT DEFINED ARCHITECTURE)
    set(ARCHITECTURE "GENERIC")
    set(ARCHITECTURE_GENERIC 1)
    add_definitions(-DARCHITECTURE_GENERIC=1)
endif()
message(STATUS "Target architecture: ${ARCHITECTURE}")


# Configure C++ standard
# ===========================

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# set up output paths for executable binaries
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)


# System imported libraries
# ======================

find_package(Boost 1.66.0 QUIET)
if (NOT Boost_FOUND)
    message(STATUS "Boost 1.66.0 or newer not found, falling back to externals")

    set(BOOST_ROOT "${PROJECT_SOURCE_DIR}/externals/boost")
    set(Boost_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/externals/boost")
    set(Boost_NO_SYSTEM_PATHS OFF)
    find_package(Boost QUIET REQUIRED)
endif()

# Prefer the -pthread flag on Linux.
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

if (VVCTRE_USE_BUNDLED_SDL2)
    # Detect toolchain and platform
    if ((MSVC_VERSION GREATER_EQUAL 1910 AND MSVC_VERSION LESS 1930) AND ARCHITECTURE_x86_64)
        set(SDL2_VER "SDL2-2.0.8")
    else()
        message(FATAL_ERROR "No bundled SDL2 binaries for your toolchain. Disable VVCTRE_USE_BUNDLED_SDL2 and provide your own.")
    endif()

    if (DEFINED SDL2_VER)
        download_bundled_external("sdl2/" ${SDL2_VER} SDL2_PREFIX)
    endif()

    set(SDL2_FOUND YES)
    set(SDL2_INCLUDE_DIR "${SDL2_PREFIX}/include" CACHE PATH "Path to SDL2 headers")
    set(SDL2_LIBRARY "${SDL2_PREFIX}/lib/x64/SDL2.lib" CACHE PATH "Path to SDL2 library")
    set(SDL2_DLL_DIR "${SDL2_PREFIX}/lib/x64/" CACHE PATH "Path to SDL2.dll")
else()
    find_package(SDL2 REQUIRED)
endif()

if (SDL2_FOUND)
    # TODO(yuriks): Make FindSDL2.cmake export an IMPORTED library instead
    add_library(SDL2 INTERFACE)
    target_link_libraries(SDL2 INTERFACE "${SDL2_LIBRARY}")
    target_include_directories(SDL2 INTERFACE "${SDL2_INCLUDE_DIR}")
endif()

if (ENABLE_FFMPEG)
    if (VVCTRE_USE_BUNDLED_FFMPEG)
        if ((MSVC_VERSION GREATER_EQUAL 1910 AND MSVC_VERSION LESS 1930) AND ARCHITECTURE_x86_64)
            set(FFmpeg_VER "ffmpeg-4.1-win64")
        else()
            message(FATAL_ERROR "No bundled FFmpeg binaries for your toolchain. Disable VVCTRE_USE_BUNDLED_FFMPEG and provide your own.")
        endif()

        if (DEFINED FFmpeg_VER)
            download_bundled_external("ffmpeg/" ${FFmpeg_VER} FFmpeg_PREFIX)
            set(FFMPEG_DIR "${FFmpeg_PREFIX}")
        endif()
    endif()

    if (ENABLE_FFMPEG_VIDEO_DUMPER)
        find_package(FFmpeg REQUIRED COMPONENTS avcodec avformat avutil swscale swresample)
    else()
        find_package(FFmpeg REQUIRED COMPONENTS avcodec)
    endif()
    if ("${FFmpeg_avcodec_VERSION}" VERSION_LESS "57.48.101")
        message(FATAL_ERROR "Found version for libavcodec is too low. The required version is at least 57.48.101 (included in FFmpeg 3.1 and later).")
    endif()
endif()

if (ENABLE_FFMPEG_VIDEO_DUMPER)
    add_definitions(-DENABLE_FFMPEG_VIDEO_DUMPER)
endif()

# Platform-specific library requirements
# ======================================

if (WIN32)
    # WSAPoll and SHGetKnownFolderPath (AppData/Roaming) didn't exist before WinNT 6.x (Vista)
    add_definitions(-D_WIN32_WINNT=0x0600 -DWINVER=0x0600)
    set(PLATFORM_LIBRARIES winmm ws2_32)
elseif (CMAKE_SYSTEM_NAME MATCHES "^(Linux|kFreeBSD|GNU|SunOS)$")
    set(PLATFORM_LIBRARIES rt)
endif()

# Setup a custom clang-format target (if clang-format can be found) that will run
# against all the src files. This should be used before making a pull request.
# =======================================================================

set(CLANG_FORMAT_POSTFIX "-6.0")
find_program(CLANG_FORMAT
    NAMES clang-format${CLANG_FORMAT_POSTFIX}
          clang-format
    PATHS ${PROJECT_BINARY_DIR}/externals)
# if find_program doesn't find it, try to download from externals
if (NOT CLANG_FORMAT)
    if (WIN32)
        message(STATUS "Clang format not found! Downloading...")
        set(CLANG_FORMAT "${PROJECT_BINARY_DIR}/externals/clang-format${CLANG_FORMAT_POSTFIX}.exe")
        file(DOWNLOAD
            https://github.com/yuzu-emu/ext-windows-bin/raw/master/clang-format${CLANG_FORMAT_POSTFIX}.exe
            "${CLANG_FORMAT}" SHOW_PROGRESS
            STATUS DOWNLOAD_SUCCESS)
        if (NOT DOWNLOAD_SUCCESS EQUAL 0)
            message(WARNING "Could not download clang format! Disabling the clang format target")
            file(REMOVE ${CLANG_FORMAT})
            unset(CLANG_FORMAT)
        endif()
    else()
        message(WARNING "Clang format not found! Disabling the clang format target")
    endif()
endif()

if (CLANG_FORMAT)
    set(SRCS ${PROJECT_SOURCE_DIR}/src)
    set(CCOMMENT "Running clang format against all the .h and .cpp files in src/")
    if (WIN32)
        add_custom_target(clang-format
            COMMAND powershell.exe -Command "Get-ChildItem '${SRCS}/*' -Include *.cpp,*.h -Recurse | Foreach {&'${CLANG_FORMAT}' -i $_.fullname}"
            COMMENT ${CCOMMENT})
    else()
        add_custom_target(clang-format
            COMMAND find ${SRCS} -iname *.h -o -iname *.cpp | xargs ${CLANG_FORMAT} -i
            COMMENT ${CCOMMENT})
    endif()
    unset(SRCS)
    unset(CCOMMENT)
endif()

# Include source code
# ===================

# This function should be passed a list of all files in a target. It will automatically generate
# file groups following the directory hierarchy, so that the layout of the files in IDEs matches the
# one in the filesystem.
function(create_target_directory_groups target_name)
    # Place any files that aren't in the source list in a separate group so that they don't get in
    # the way.
    source_group("Other Files" REGULAR_EXPRESSION ".")

    get_target_property(target_sources "${target_name}" SOURCES)

    foreach(file_name IN LISTS target_sources)
        get_filename_component(dir_name "${file_name}" PATH)
        # Group names use '\' as a separator even though the entire rest of CMake uses '/'...
        string(REPLACE "/" "\\" group_name "${dir_name}")
        source_group("${group_name}" FILES "${file_name}")
    endforeach()
endfunction()

# Gets a UTC timstamp and sets the provided variable to it
function(get_timestamp _var)
    string(TIMESTAMP timestamp UTC)
    set(${_var} "${timestamp}" PARENT_SCOPE)
endfunction()

# Prevent boost from linking against libs when building
add_definitions(-DBOOST_ERROR_CODE_HEADER_ONLY
    -DBOOST_SYSTEM_NO_LIB
    -DBOOST_DATE_TIME_NO_LIB
    -DBOOST_REGEX_NO_LIB
)

# generate git/build information
include(GetGitRevisionDescription)
get_git_head_revision(GIT_REF_SPEC GIT_REV)
git_describe(GIT_DESC --always --long --dirty)
git_branch_name(GIT_BRANCH)
get_timestamp(BUILD_DATE)

enable_testing()
add_subdirectory(externals)
add_subdirectory(src)

# Set vvctre project as default StartUp Project in Visual Studio
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})
