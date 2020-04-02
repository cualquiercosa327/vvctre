set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/CMakeModules)

add_executable(${PROJECT_NAME}
    ${PROJECT_NAME}.cpp
    ${PROJECT_NAME}.rc
    emu_window/emu_window_sdl2.cpp
    emu_window/emu_window_sdl2.h
    resource.h
    applets/swkbd.cpp
    applets/swkbd.h
    applets/mii_selector.cpp
    applets/mii_selector.h
    camera/image.cpp
    camera/image.h
    camera/util.cpp
    camera/util.h
    configuration.cpp
    configuration.h
)

create_target_directory_groups(${PROJECT_NAME})

target_link_libraries(${PROJECT_NAME} PRIVATE common core input_common audio_core)
target_link_libraries(${PROJECT_NAME} PRIVATE glad clip portable-file-dialogs indicators imgui json ${PLATFORM_LIBRARIES} Threads::Threads)

if(UNIX)
    install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION "${CMAKE_INSTALL_PREFIX}/bin")
endif()

if (MSVC)
    include(CopySDLDeps)
    copy_SDL_deps(${PROJECT_NAME})
endif()
