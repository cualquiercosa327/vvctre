// Copyright 2020 vvctre project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"

#ifndef VVCTRE_PLUGIN_FUNCTION
#ifdef _WIN32
#define VVCTRE_PLUGIN_FUNCTION extern "C" __declspec(dllimport)
#else
#define VVCTRE_PLUGIN_FUNCTION extern "C"
#endif
#endif

// File
VVCTRE_PLUGIN_FUNCTION void vvctre_load_file(void* core, const char* path);
VVCTRE_PLUGIN_FUNCTION bool vvctre_install_cia(const char* path);

VVCTRE_PLUGIN_FUNCTION bool vvctre_load_amiibo(void* core, const char* path);
VVCTRE_PLUGIN_FUNCTION void vvctre_remove_amiibo(void* core);

// Emulation
VVCTRE_PLUGIN_FUNCTION void vvctre_restart(void* core);
VVCTRE_PLUGIN_FUNCTION void vvctre_set_paused(void* plugin_manager, bool paused);
VVCTRE_PLUGIN_FUNCTION bool vvctre_get_paused(void* plugin_manager);

// Memory
VVCTRE_PLUGIN_FUNCTION u8 vvctre_read_u8(void* core, VAddr address);
VVCTRE_PLUGIN_FUNCTION void vvctre_write_u8(void* core, VAddr address, u8 value);

VVCTRE_PLUGIN_FUNCTION u16 vvctre_read_u16(void* core, VAddr address);
VVCTRE_PLUGIN_FUNCTION void vvctre_write_u16(void* core, VAddr address, u16 value);

VVCTRE_PLUGIN_FUNCTION u32 vvctre_read_u32(void* core, VAddr address);
VVCTRE_PLUGIN_FUNCTION void vvctre_write_u32(void* core, VAddr address, u32 value);

VVCTRE_PLUGIN_FUNCTION u64 vvctre_read_u64(void* core, VAddr address);
VVCTRE_PLUGIN_FUNCTION void vvctre_write_u64(void* core, VAddr address, u64 value);

// Debugging
VVCTRE_PLUGIN_FUNCTION void vvctre_set_pc(void* core, u32 addr);
VVCTRE_PLUGIN_FUNCTION u32 vvctre_get_pc(void* core);

VVCTRE_PLUGIN_FUNCTION void vvctre_set_register(void* core, int index, u32 value);
VVCTRE_PLUGIN_FUNCTION u32 vvctre_get_register(void* core, int index);

VVCTRE_PLUGIN_FUNCTION void vvctre_set_vfp_register(void* core, int index, u32 value);
VVCTRE_PLUGIN_FUNCTION u32 vvctre_get_vfp_register(void* core, int index);

VVCTRE_PLUGIN_FUNCTION void vvctre_set_vfp_system_register(void* core, int index, u32 value);
VVCTRE_PLUGIN_FUNCTION u32 vvctre_get_vfp_system_register(void* core, int index);

VVCTRE_PLUGIN_FUNCTION void vvctre_set_cp15_register(void* core, int index, u32 value);
VVCTRE_PLUGIN_FUNCTION u32 vvctre_get_cp15_register(void* core, int index);

// Cheats
VVCTRE_PLUGIN_FUNCTION int vvctre_cheat_count(void* core);

VVCTRE_PLUGIN_FUNCTION const char* vvctre_get_cheat(void* core, int index);
VVCTRE_PLUGIN_FUNCTION const char* vvctre_get_cheat_name(void* core, int index);
VVCTRE_PLUGIN_FUNCTION const char* vvctre_get_cheat_comments(void* core, int index);
VVCTRE_PLUGIN_FUNCTION const char* vvctre_get_cheat_type(void* core, int index);
VVCTRE_PLUGIN_FUNCTION const char* vvctre_get_cheat_code(void* core, int index);

VVCTRE_PLUGIN_FUNCTION void vvctre_set_cheat_enabled(void* core, int index, bool enabled);

VVCTRE_PLUGIN_FUNCTION void vvctre_add_gateway_cheat(void* core, const char* name, const char* code,
                                                     const char* comments);
VVCTRE_PLUGIN_FUNCTION void vvctre_remove_cheat(void* core, int index);
VVCTRE_PLUGIN_FUNCTION void vvctre_update_gateway_cheat(void* core, int index, const char* name,
                                                        const char* code, const char* comments);

// Camera
VVCTRE_PLUGIN_FUNCTION void vvctre_reload_camera_images(void* core);

// GUI
VVCTRE_PLUGIN_FUNCTION void vvctre_gui_text(const char* text);
VVCTRE_PLUGIN_FUNCTION bool vvctre_gui_button(const char* text);

VVCTRE_PLUGIN_FUNCTION bool vvctre_gui_begin(const char* name);
VVCTRE_PLUGIN_FUNCTION void vvctre_gui_end();

VVCTRE_PLUGIN_FUNCTION bool vvctre_gui_begin_menu(const char* name);
VVCTRE_PLUGIN_FUNCTION void vvctre_gui_end_menu();

VVCTRE_PLUGIN_FUNCTION bool vvctre_gui_menu_item(const char* name);

// Button devices
VVCTRE_PLUGIN_FUNCTION void* vvctre_button_device_new(void* plugin_manager, const char* params);
VVCTRE_PLUGIN_FUNCTION void vvctre_button_device_delete(void* plugin_manager, void* device);
VVCTRE_PLUGIN_FUNCTION bool vvctre_button_device_get_state(void* device);

// TAS
VVCTRE_PLUGIN_FUNCTION void vvctre_movie_prepare_for_playback(const char* path);
VVCTRE_PLUGIN_FUNCTION void vvctre_movie_prepare_for_recording();
VVCTRE_PLUGIN_FUNCTION void vvctre_movie_play(const char* path);
VVCTRE_PLUGIN_FUNCTION void vvctre_movie_record(const char* path);
VVCTRE_PLUGIN_FUNCTION bool vvctre_movie_is_playing();
VVCTRE_PLUGIN_FUNCTION bool vvctre_movie_is_recording();
VVCTRE_PLUGIN_FUNCTION void vvctre_movie_stop();

VVCTRE_PLUGIN_FUNCTION void vvctre_set_frame_advancing_enabled(void* core, bool enabled);
VVCTRE_PLUGIN_FUNCTION bool vvctre_get_frame_advancing_enabled(void* core);
VVCTRE_PLUGIN_FUNCTION void vvctre_advance_frame(void* core);

// Remote control
VVCTRE_PLUGIN_FUNCTION void vvctre_set_custom_pad_state(void* core, u32 state);
VVCTRE_PLUGIN_FUNCTION void vvctre_use_real_pad_state(void* core);

VVCTRE_PLUGIN_FUNCTION void vvctre_set_custom_circle_pad_state(void* core, float x, float y);
VVCTRE_PLUGIN_FUNCTION void vvctre_use_real_circle_pad_state(void* core);

VVCTRE_PLUGIN_FUNCTION void vvctre_set_custom_touch_state(void* core, float x, float y,
                                                          bool pressed);
VVCTRE_PLUGIN_FUNCTION void vvctre_use_real_touch_state(void* core);

VVCTRE_PLUGIN_FUNCTION void vvctre_set_custom_motion_state(void* core, float accelerometer[3],
                                                           float gyroscope[3]);
VVCTRE_PLUGIN_FUNCTION void vvctre_use_real_motion_state(void* core);

// Settings
VVCTRE_PLUGIN_FUNCTION void vvctre_settings_apply();
VVCTRE_PLUGIN_FUNCTION void vvctre_settings_log();

// Start Settings
VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_file_path(const char* value);
VVCTRE_PLUGIN_FUNCTION const char* vvctre_settings_get_file_path();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_play_movie(const char* value);
VVCTRE_PLUGIN_FUNCTION const char* vvctre_settings_get_play_movie();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_record_movie(const char* value);
VVCTRE_PLUGIN_FUNCTION const char* vvctre_settings_get_record_movie();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_region_value(int value);
VVCTRE_PLUGIN_FUNCTION int vvctre_settings_get_region_value();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_log_filter(const char* value);
VVCTRE_PLUGIN_FUNCTION const char* vvctre_settings_get_log_filter();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_multiplayer_url(const char* value);
VVCTRE_PLUGIN_FUNCTION const char* vvctre_settings_get_multiplayer_url();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_initial_clock(int value);
VVCTRE_PLUGIN_FUNCTION int vvctre_settings_get_initial_clock();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_unix_timestamp(u64 value);
VVCTRE_PLUGIN_FUNCTION u64 vvctre_settings_get_unix_timestamp();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_use_virtual_sd(bool value);
VVCTRE_PLUGIN_FUNCTION bool vvctre_settings_get_use_virtual_sd();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_start_in_fullscreen_mode(bool value);
VVCTRE_PLUGIN_FUNCTION bool vvctre_settings_get_start_in_fullscreen_mode();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_record_frame_times(bool value);
VVCTRE_PLUGIN_FUNCTION bool vvctre_settings_get_record_frame_times();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_enable_gdbstub(u16 port);
VVCTRE_PLUGIN_FUNCTION void vvctre_settings_disable_gdbstub();

VVCTRE_PLUGIN_FUNCTION bool vvctre_settings_is_gdb_stub_enabled();
VVCTRE_PLUGIN_FUNCTION u16 vvctre_settings_get_gdb_stub_port();

// General Settings
VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_use_cpu_jit(bool value);
VVCTRE_PLUGIN_FUNCTION bool vvctre_settings_get_use_cpu_jit();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_limit_speed(bool value);
VVCTRE_PLUGIN_FUNCTION bool vvctre_settings_get_limit_speed();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_speed_limit(u16 value);
VVCTRE_PLUGIN_FUNCTION u16 vvctre_settings_get_speed_limit();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_use_custom_cpu_ticks(bool value);
VVCTRE_PLUGIN_FUNCTION bool vvctre_settings_get_use_custom_cpu_ticks();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_custom_cpu_ticks(u64 value);
VVCTRE_PLUGIN_FUNCTION u64 vvctre_settings_get_custom_cpu_ticks();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_cpu_clock_percentage(u32 value);
VVCTRE_PLUGIN_FUNCTION u32 vvctre_settings_get_cpu_clock_percentage();

// Audio Settings
VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_enable_dsp_lle(bool value);
VVCTRE_PLUGIN_FUNCTION bool vvctre_settings_get_enable_dsp_lle();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_enable_dsp_lle_multithread(bool value);
VVCTRE_PLUGIN_FUNCTION bool vvctre_settings_get_enable_dsp_lle_multithread();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_audio_volume(float value);
VVCTRE_PLUGIN_FUNCTION float vvctre_settings_get_audio_volume();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_audio_sink_id(const char* value);
VVCTRE_PLUGIN_FUNCTION const char* vvctre_settings_get_audio_sink_id();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_audio_device_id(const char* value);
VVCTRE_PLUGIN_FUNCTION const char* vvctre_settings_get_audio_device_id();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_microphone_input_type(int value);
VVCTRE_PLUGIN_FUNCTION int vvctre_settings_get_microphone_input_type();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_microphone_device(const char* value);
VVCTRE_PLUGIN_FUNCTION const char* vvctre_settings_get_microphone_device();

// Camera Settings
VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_camera_engine(int index, const char* value);
VVCTRE_PLUGIN_FUNCTION const char* vvctre_settings_get_camera_engine(int index);

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_camera_parameter(int index, const char* value);
VVCTRE_PLUGIN_FUNCTION const char* vvctre_settings_get_camera_parameter(int index);

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_camera_flip(int index, int value);
VVCTRE_PLUGIN_FUNCTION int vvctre_settings_get_camera_flip(int index);

// Graphics Settings
VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_use_hardware_renderer(bool value);
VVCTRE_PLUGIN_FUNCTION bool vvctre_settings_get_use_hardware_renderer();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_use_hardware_shader(bool value);
VVCTRE_PLUGIN_FUNCTION bool vvctre_settings_get_use_hardware_shader();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_hardware_shader_accurate_multiplication(bool value);
VVCTRE_PLUGIN_FUNCTION bool vvctre_settings_get_hardware_shader_accurate_multiplication();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_use_shader_jit(bool value);
VVCTRE_PLUGIN_FUNCTION bool vvctre_settings_get_use_shader_jit();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_enable_vsync(bool value);
VVCTRE_PLUGIN_FUNCTION bool vvctre_settings_get_enable_vsync();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_dump_textures(bool value);
VVCTRE_PLUGIN_FUNCTION bool vvctre_settings_get_dump_textures();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_custom_textures(bool value);
VVCTRE_PLUGIN_FUNCTION bool vvctre_settings_get_custom_textures();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_preload_textures(bool value);
VVCTRE_PLUGIN_FUNCTION bool vvctre_settings_get_preload_textures();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_enable_linear_filtering(bool value);
VVCTRE_PLUGIN_FUNCTION bool vvctre_settings_get_enable_linear_filtering();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_sharper_distant_objects(bool value);
VVCTRE_PLUGIN_FUNCTION bool vvctre_settings_get_sharper_distant_objects();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_resolution(u16 value);
VVCTRE_PLUGIN_FUNCTION u16 vvctre_settings_get_resolution();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_background_color_red(float value);
VVCTRE_PLUGIN_FUNCTION float vvctre_settings_get_background_color_red();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_background_color_green(float value);
VVCTRE_PLUGIN_FUNCTION float vvctre_settings_get_background_color_green();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_background_color_blue(float value);
VVCTRE_PLUGIN_FUNCTION float vvctre_settings_get_background_color_blue();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_post_processing_shader(const char* value);
VVCTRE_PLUGIN_FUNCTION const char* vvctre_settings_get_post_processing_shader();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_texture_filter(const char* value);
VVCTRE_PLUGIN_FUNCTION const char* vvctre_settings_get_texture_filter();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_render_3d(int value);
VVCTRE_PLUGIN_FUNCTION int vvctre_settings_get_render_3d();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_factor_3d(u8 value);
VVCTRE_PLUGIN_FUNCTION u8 vvctre_settings_get_factor_3d();

// Controls Settings
VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_button(int index, const char* params);
VVCTRE_PLUGIN_FUNCTION const char* vvctre_settings_get_button(int index);

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_analog(int index, const char* params);
VVCTRE_PLUGIN_FUNCTION const char* vvctre_settings_get_analog(int index);

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_motion_device(const char* params);
VVCTRE_PLUGIN_FUNCTION const char* vvctre_settings_get_motion_device();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_touch_device(const char* params);
VVCTRE_PLUGIN_FUNCTION const char* vvctre_settings_get_touch_device();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_cemuhookudp_address(const char* value);
VVCTRE_PLUGIN_FUNCTION const char* vvctre_settings_get_cemuhookudp_address();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_cemuhookudp_port(u16 value);
VVCTRE_PLUGIN_FUNCTION u16 vvctre_settings_get_cemuhookudp_port();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_cemuhookudp_pad_index(u8 value);
VVCTRE_PLUGIN_FUNCTION u8 vvctre_settings_get_cemuhookudp_pad_index();

// Layout Settings
VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_layout(int value);
VVCTRE_PLUGIN_FUNCTION int vvctre_settings_get_layout();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_swap_screens(bool value);
VVCTRE_PLUGIN_FUNCTION bool vvctre_settings_get_swap_screens();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_upright_screens(bool value);
VVCTRE_PLUGIN_FUNCTION bool vvctre_settings_get_upright_screens();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_use_custom_layout(bool value);
VVCTRE_PLUGIN_FUNCTION bool vvctre_settings_get_use_custom_layout();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_custom_layout_top_left(u16 value);
VVCTRE_PLUGIN_FUNCTION u16 vvctre_settings_get_custom_layout_top_left();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_custom_layout_top_top(u16 value);
VVCTRE_PLUGIN_FUNCTION u16 vvctre_settings_get_custom_layout_top_top();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_custom_layout_top_right(u16 value);
VVCTRE_PLUGIN_FUNCTION u16 vvctre_settings_get_custom_layout_top_right();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_custom_layout_top_bottom(u16 value);
VVCTRE_PLUGIN_FUNCTION u16 vvctre_settings_get_custom_layout_top_bottom();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_custom_layout_bottom_left(u16 value);
VVCTRE_PLUGIN_FUNCTION u16 vvctre_settings_get_custom_layout_bottom_left();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_custom_layout_bottom_top(u16 value);
VVCTRE_PLUGIN_FUNCTION u16 vvctre_settings_get_custom_layout_bottom_top();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_custom_layout_bottom_right(u16 value);
VVCTRE_PLUGIN_FUNCTION u16 vvctre_settings_get_custom_layout_bottom_right();

VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_custom_layout_bottom_bottom(u16 value);
VVCTRE_PLUGIN_FUNCTION u16 vvctre_settings_get_custom_layout_bottom_bottom();

// LLE Modules Settings
VVCTRE_PLUGIN_FUNCTION void vvctre_settings_set_use_lle_module(const char* name, bool value);
VVCTRE_PLUGIN_FUNCTION bool vvctre_settings_get_use_lle_module(const char* name);

// Other
VVCTRE_PLUGIN_FUNCTION const char* vvctre_get_version();
VVCTRE_PLUGIN_FUNCTION bool vvctre_emulation_running(void* core);

VVCTRE_PLUGIN_FUNCTION void vvctre_set_play_coins(u16 value);
VVCTRE_PLUGIN_FUNCTION u16 vvctre_get_play_coins();
