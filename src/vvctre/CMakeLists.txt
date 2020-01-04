set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/CMakeModules)

add_executable(${PROJECT_NAME}
    ${PROJECT_NAME}.cpp
    ${PROJECT_NAME}.rc
    config.cpp
    config.h
    default_ini.h
    emu_window/emu_window_sdl2.cpp
    emu_window/emu_window_sdl2.h
    lodepng_image_interface.cpp
    lodepng_image_interface.h
    resource.h
)

create_target_directory_groups(${PROJECT_NAME})

target_link_libraries(${PROJECT_NAME} PRIVATE common core input_common network)
target_link_libraries(${PROJECT_NAME} PRIVATE inih glad lodepng clipp ${PLATFORM_LIBRARIES} SDL2 Threads::Threads)

if(UNIX AND NOT APPLE)
    install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION "${CMAKE_INSTALL_PREFIX}/bin")
endif()

if (MSVC)
    include(CopySDLDeps)
    copy_SDL_deps(${PROJECT_NAME})

    if (ENABLE_FFMPEG)
        include(CopyFFmpegDeps)
        copy_FFmpeg_deps(${PROJECT_NAME})
    endif()
endif()
