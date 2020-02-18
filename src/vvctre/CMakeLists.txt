set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/CMakeModules)

add_executable(${PROJECT_NAME}
    ${PROJECT_NAME}.cpp
    ${PROJECT_NAME}.rc
    config.cpp
    config.h
    default_ini.h
    emu_window/emu_window_sdl2.cpp
    emu_window/emu_window_sdl2.h
    resource.h
    applets/swkbd.cpp
    applets/swkbd.h
    applets/mii_selector.cpp
    applets/mii_selector.h
    $<$<BOOL:${USE_DISCORD_PRESENCE}>:discord_rp.cpp discord_rp.h>
)

create_target_directory_groups(${PROJECT_NAME})

target_link_libraries(${PROJECT_NAME} PRIVATE common core input_common)
target_link_libraries(${PROJECT_NAME} PRIVATE inih glad clipp portable-file-dialogs indicators ${PLATFORM_LIBRARIES} SDL2 Threads::Threads)

if(USE_DISCORD_PRESENCE)
    target_compile_definitions(${PROJECT_NAME} PRIVATE -DUSE_DISCORD_PRESENCE)
    target_link_libraries(${PROJECT_NAME} PRIVATE discord-rpc)
endif()

if(UNIX)
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
