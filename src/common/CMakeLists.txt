add_library(common STATIC
    alignment.h
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
    scope_exit.h
    string_util.cpp
    string_util.h
    swap.h
    texture.cpp
    texture.h
    thread.cpp
    thread.h
    thread_pool.h
    thread_queue_list.h
    threadsafe_queue.h
    timer.cpp
    timer.h
    vector_math.h
    zstd_compression.cpp
    zstd_compression.h
    version.cpp
    version.h
    stb_image.cpp
    stb_image.h
    stb_image_write.cpp
    stb_image_write.h
    stb_image_resize.cpp
    stb_image_resize.h
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

target_link_libraries(common PUBLIC fmt semver PRIVATE libzstd_static)
target_include_directories(common PRIVATE ${PROJECT_SOURCE_DIR}/externals/zstd/lib)

if (ARCHITECTURE_x86_64)
    target_link_libraries(common PRIVATE xbyak)
endif()
