# Generate cpp with Git revision from template
# Also if this is a CI build, add the build name (ie: Nightly, Canary) to the scm_rev file as well
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/scm_rev.cpp.in" "${CMAKE_CURRENT_SOURCE_DIR}/scm_rev.cpp" @ONLY)

add_library(common STATIC
    alignment.h
    announce_multiplayer_room.h
    assert.h
    detached_tasks.cpp
    detached_tasks.h
    bit_field.h
    bit_set.h
    cityhash.cpp
    cityhash.h
    color.h
    common_funcs.h
    common_paths.h
    common_types.h
    file_util.cpp
    file_util.h
    hash.h
    logging/backend.cpp
    logging/backend.h
    logging/filter.cpp
    logging/filter.h
    logging/log.h
    logging/text_formatter.cpp
    logging/text_formatter.h
    math_util.h
    misc.cpp
    param_package.cpp
    param_package.h
    quaternion.h
    ring_buffer.h
    scm_rev.cpp
    scm_rev.h
    scope_exit.h
    string_util.cpp
    string_util.h
    swap.h
    texture.cpp
    texture.h
    thread.cpp
    thread.h
    thread_queue_list.h
    threadsafe_queue.h
    timer.cpp
    timer.h
    vector_math.h
)

if(ARCHITECTURE_x86_64)
    target_sources(common
        PRIVATE
            x64/cpu_detect.cpp

            x64/cpu_detect.h
            x64/xbyak_abi.h
            x64/xbyak_util.h
    )
endif()

create_target_directory_groups(common)

target_link_libraries(common PUBLIC fmt)
if (ARCHITECTURE_x86_64)
    target_link_libraries(common PRIVATE xbyak)
endif()
