# Definitions for all external bundled libraries

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/CMakeModules)
include(DownloadExternals)
include(ExternalProject)

# Catch
add_library(catch-single-include INTERFACE)
target_include_directories(catch-single-include INTERFACE catch/single_include)

# Crypto++
add_subdirectory(cryptopp)

# Dynarmic
if (ARCHITECTURE_x86_64)
    # Dynarmic will skip defining xbyak if it's already defined, we then define it below
    add_library(xbyak INTERFACE)
    option(DYNARMIC_TESTS OFF)
    set(DYNARMIC_NO_BUNDLED_FMT ON)
    add_subdirectory(dynarmic)
endif()

# libfmt
add_subdirectory(fmt)
add_library(fmt::fmt ALIAS fmt)

# Glad
add_subdirectory(glad)

# Nihstro
add_library(nihstro-headers INTERFACE)
target_include_directories(nihstro-headers INTERFACE ./nihstro/include)

# Open Source Archives
add_subdirectory(open_source_archives)

# SoundTouch
add_subdirectory(soundtouch)
# The SoundTouch target doesn't export the necessary include paths as properties by default
target_include_directories(SoundTouch INTERFACE ./soundtouch/include)

# Teakra
add_subdirectory(teakra EXCLUDE_FROM_ALL)

# Xbyak
if (ARCHITECTURE_x86_64)
    # Defined before "dynarmic" above
    # add_library(xbyak INTERFACE)
    target_include_directories(xbyak INTERFACE ./xbyak/xbyak)
    target_compile_definitions(xbyak INTERFACE XBYAK_NO_OP_NAMES)
endif()

# Cubeb
if (ENABLE_CUBEB)
    set(BUILD_TESTS OFF CACHE BOOL "")
    add_subdirectory(cubeb EXCLUDE_FROM_ALL)
endif()

# LibreSSL
set(LIBRESSL_SKIP_INSTALL ON CACHE BOOL "")
add_subdirectory(libressl EXCLUDE_FROM_ALL)
target_include_directories(ssl INTERFACE ./libressl/include)
target_compile_definitions(ssl PRIVATE -DHAVE_INET_NTOP)

# lurlparser
add_subdirectory(lurlparser EXCLUDE_FROM_ALL)

# httplib
add_library(httplib INTERFACE)
target_include_directories(httplib INTERFACE ./httplib)

# JSON
add_library(json INTERFACE)
target_include_directories(json INTERFACE ./json)

# portable-file-dialogs
add_library(portable-file-dialogs INTERFACE)
target_include_directories(portable-file-dialogs INTERFACE ./portable-file-dialogs/include)

# Dear ImGui
add_library(imgui
    imgui/imconfig.h
    imgui/imgui_draw.cpp
    imgui/imgui_internal.h
    imgui/imgui_widgets.cpp
    imgui/imgui.cpp
    imgui/imgui.h
    imgui/imstb_rectpack.h
    imgui/imstb_textedit.h
    imgui/imstb_truetype.h
    imgui/examples/imgui_impl_opengl3.cpp
    imgui/examples/imgui_impl_opengl3.h
    imgui/examples/imgui_impl_sdl.cpp
    imgui/examples/imgui_impl_sdl.h
    imgui/misc/cpp/imgui_stdlib.cpp
    imgui/misc/cpp/imgui_stdlib.h
)
target_link_libraries(imgui PRIVATE glad SDL2)
target_include_directories(imgui INTERFACE ./imgui ./imgui/examples ./imgui/misc/cpp)
target_include_directories(imgui PRIVATE ./imgui ./imgui/examples ./imgui/misc/cpp)

# clip
set(CLIP_EXAMPLES OFF CACHE BOOL "")
set(CLIP_TESTS OFF CACHE BOOL "")
if(UNIX)
    set(CLIP_X11_WITH_PNG ON)
endif()
add_subdirectory(clip EXCLUDE_FROM_ALL)
target_include_directories(clip INTERFACE ./clip)

add_subdirectory(semver)
add_subdirectory(easywsclient)
add_subdirectory(indicators)
