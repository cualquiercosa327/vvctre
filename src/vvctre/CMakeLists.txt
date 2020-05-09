set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/CMakeModules)

add_executable(${PROJECT_NAME}
    ${PROJECT_NAME}.cpp
    ${PROJECT_NAME}.rc
    common.h
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
    function_logger.cpp
    function_logger.h
    initial_settings.cpp
    initial_settings.h
    plugins.cpp
    plugins.h
)

create_target_directory_groups(${PROJECT_NAME})

get_directory_property(OPENSSL_LIBS
    DIRECTORY ${PROJECT_SOURCE_DIR}/externals/libressl
    DEFINITION OPENSSL_LIBS)

target_link_libraries(${PROJECT_NAME} PRIVATE common core input_common audio_core)
target_link_libraries(${PROJECT_NAME} PRIVATE glad clip portable-file-dialogs imgui json ${OPENSSL_LIBS} httplib lurlparser ${PLATFORM_LIBRARIES} Threads::Threads)
target_compile_definitions(${PROJECT_NAME} PRIVATE -DCPPHTTPLIB_OPENSSL_SUPPORT)

if (MSVC)
    include(CopySDLDeps)
    copy_SDL_deps(${PROJECT_NAME})
endif()
