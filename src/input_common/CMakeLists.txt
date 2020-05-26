add_library(input_common STATIC
    analog_from_button.cpp
    analog_from_button.h
    keyboard.cpp
    keyboard.h
    main.cpp
    main.h
    motion_emu.cpp
    motion_emu.h
    sdl/sdl.cpp
    sdl/sdl.h
    sdl/sdl_impl.cpp
    sdl/sdl_impl.h
    udp/client.cpp
    udp/client.h
    udp/protocol.cpp
    udp/protocol.h
    udp/udp.cpp
    udp/udp.h
)

create_target_directory_groups(input_common)

target_link_libraries(input_common PUBLIC core PRIVATE common ${Boost_LIBRARIES})
