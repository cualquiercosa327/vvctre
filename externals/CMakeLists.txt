# Definitions for all external bundled libraries

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/CMakeModules)
include(DownloadExternals)
include(ExternalProject)

# Dynarmic
if (ARCHITECTURE_x86_64)
    # Dynarmic will skip defining xbyak if it's already defined, we then define it below
    add_library(xbyak INTERFACE)
    option(DYNARMIC_TESTS OFF)
    set(DYNARMIC_NO_BUNDLED_FMT ON)
    set(DYNARMIC_FRONTENDS "A32")
    add_subdirectory(dynarmic)
endif()

# libfmt
add_subdirectory(fmt)
add_library(fmt::fmt ALIAS fmt)

# Nihstro
add_library(nihstro-headers INTERFACE)
target_include_directories(nihstro-headers INTERFACE ./nihstro/include)

# SoundTouch
add_subdirectory(soundtouch)
# The SoundTouch target doesn't export the necessary include paths as properties by default
target_include_directories(SoundTouch INTERFACE ./soundtouch/include)

# Xbyak
if (ARCHITECTURE_x86_64)
    # Defined before "dynarmic" above
    # add_library(xbyak INTERFACE)
    target_include_directories(xbyak INTERFACE ./xbyak/xbyak)
    target_compile_definitions(xbyak INTERFACE XBYAK_NO_OP_NAMES)
endif()

# ENet
add_subdirectory(enet)
target_include_directories(enet INTERFACE ./enet/include)

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
add_library(lurlparser
    lurlparser/LUrlParser.cpp
    lurlparser/LUrlParser.h)
target_include_directories(lurlparser INTERFACE ./lurlparser)

# httplib
add_library(httplib INTERFACE)
target_include_directories(httplib INTERFACE ./httplib)

# OpenGL loader
add_library(opengl-loader STATIC
    opengl-loader/src/glad.c
    opengl-loader/include/KHR/khrplatform.h
    opengl-loader/include/glad/glad.h)
target_include_directories(opengl-loader PUBLIC ./opengl-loader/include)
if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    target_link_libraries(opengl-loader PRIVATE dl)
endif()

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
    imgui/misc/cpp/imgui_stdlib.h)
target_link_libraries(imgui PRIVATE opengl-loader SDL2)
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

# stb
add_library(stb
    stb/stb_image.h
    stb/stb_image_write.h
    stb/stb_image_resize.h
    stb_image.cpp
    stb_image_write.cpp
    stb_image_resize.cpp
)
target_include_directories(stb PUBLIC ./stb)

# json
set(JSON_BuildTests OFF CACHE BOOL "")
set(JSON_Install OFF CACHE BOOL "")
add_subdirectory(json EXCLUDE_FROM_ALL)

add_subdirectory(cryptopp)
add_subdirectory(system-archives)
add_subdirectory(teakra EXCLUDE_FROM_ALL)
add_subdirectory(portable-file-dialogs EXCLUDE_FROM_ALL)
