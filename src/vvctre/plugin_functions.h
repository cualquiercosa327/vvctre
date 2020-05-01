// Copyright 2020 vvctre project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"

// File
void vvctre_load_file(void* core, const char* path);
bool vvctre_install_cia(const char* path);

bool vvctre_load_amiibo(void* core, const char* path);
void vvctre_remove_amiibo(void* core);

// Emulation
void vvctre_restart(void* core);
void vvctre_set_paused(void* plugin_manager, bool paused);
bool vvctre_get_paused(void* plugin_manager);

// Memory
u8 vvctre_read_u8(void* core, VAddr address);
void vvctre_write_u8(void* core, VAddr address, u8 value);

u16 vvctre_read_u16(void* core, VAddr address);
void vvctre_write_u16(void* core, VAddr address, u16 value);

u32 vvctre_read_u32(void* core, VAddr address);
void vvctre_write_u32(void* core, VAddr address, u32 value);

u64 vvctre_read_u64(void* core, VAddr address);
void vvctre_write_u64(void* core, VAddr address, u64 value);

// Debugging
void vvctre_set_pc(void* core, u32 addr);
u32 vvctre_get_pc(void* core);

void vvctre_set_register(void* core, int index, u32 value);
u32 vvctre_get_register(void* core, int index);

void vvctre_set_vfp_register(void* core, int index, u32 value);
u32 vvctre_get_vfp_register(void* core, int index);

void vvctre_set_vfp_system_register(void* core, int index, u32 value);
u32 vvctre_get_vfp_system_register(void* core, int index);

void vvctre_set_cp15_register(void* core, int index, u32 value);
u32 vvctre_get_cp15_register(void* core, int index);

// Cheats
int vvctre_cheat_count(void* core);

const char* vvctre_get_cheat(void* core, int index);
const char* vvctre_get_cheat_name(void* core, int index);
const char* vvctre_get_cheat_comments(void* core, int index);
const char* vvctre_get_cheat_type(void* core, int index);
const char* vvctre_get_cheat_code(void* core, int index);

void vvctre_set_cheat_enabled(void* core, int index, bool enabled);

void vvctre_add_gateway_cheat(void* core, const char* name, const char* code, const char* comments);
void vvctre_remove_cheat(void* core, int index);
void vvctre_update_gateway_cheat(void* core, int index, const char* name, const char* code,
                                 const char* comments);

// Camera
void vvctre_reload_camera_images(void* core);

// GUI
void vvctre_gui_text(const char* text);
bool vvctre_gui_button(const char* text);

bool vvctre_gui_begin(const char* name);
void vvctre_gui_end();

bool vvctre_gui_begin_menu(const char* name);
void vvctre_gui_end_menu();

bool vvctre_gui_menu_item(const char* name);

// Button devices
void* vvctre_button_device_new(void* plugin_manager, const char* params);
void vvctre_button_device_delete(void* plugin_manager, void* device);
bool vvctre_button_device_get_state(void* device);

// TAS
void vvctre_movie_prepare_for_playback(const char* path);
void vvctre_movie_prepare_for_recording();
void vvctre_movie_play(const char* path);
void vvctre_movie_record(const char* path);
bool vvctre_movie_is_playing();
bool vvctre_movie_is_recording();
void vvctre_movie_stop();

void vvctre_set_frame_advancing_enabled(void* core, bool enabled);
bool vvctre_get_frame_advancing_enabled(void* core);
void vvctre_advance_frame(void* core);

// Remote control
void vvctre_set_custom_pad_state(void* core, u32 state);
void vvctre_use_real_pad_state(void* core);

void vvctre_set_custom_circle_pad_state(void* core, float x, float y);
void vvctre_use_real_circle_pad_state(void* core);

void vvctre_set_custom_touch_state(void* core, float x, float y, bool pressed);
void vvctre_use_real_touch_state(void* core);

void vvctre_set_custom_motion_state(void* core, float accelerometer[3], float gyroscope[3]);
void vvctre_use_real_motion_state(void* core);

// Settings
void vvctre_settings_apply();
void vvctre_settings_log();

// Start Settings
void vvctre_settings_set_file_path(const char* value);
const char* vvctre_settings_get_file_path();

void vvctre_settings_set_play_movie(const char* value);
const char* vvctre_settings_get_play_movie();

void vvctre_settings_set_record_movie(const char* value);
const char* vvctre_settings_get_record_movie();

void vvctre_settings_set_region_value(int value);
int vvctre_settings_get_region_value();

void vvctre_settings_set_log_filter(const char* value);
const char* vvctre_settings_get_log_filter();

void vvctre_settings_set_multiplayer_url(const char* value);
const char* vvctre_settings_get_multiplayer_url();

void vvctre_settings_set_initial_clock(int value);
int vvctre_settings_get_initial_clock();

void vvctre_settings_set_unix_timestamp(u64 value);
u64 vvctre_settings_get_unix_timestamp();

void vvctre_settings_set_use_virtual_sd(bool value);
bool vvctre_settings_get_use_virtual_sd();

void vvctre_settings_set_start_in_fullscreen_mode(bool value);
bool vvctre_settings_get_start_in_fullscreen_mode();

void vvctre_settings_set_record_frame_times(bool value);
bool vvctre_settings_get_record_frame_times();

void vvctre_settings_enable_gdbstub(u16 port);
void vvctre_settings_disable_gdbstub();

bool vvctre_settings_is_gdb_stub_enabled();
u16 vvctre_settings_get_gdb_stub_port();

// General Settings
void vvctre_settings_set_use_cpu_jit(bool value);
bool vvctre_settings_get_use_cpu_jit();

void vvctre_settings_set_limit_speed(bool value);
bool vvctre_settings_get_limit_speed();

void vvctre_settings_set_speed_limit(u16 value);
u16 vvctre_settings_get_speed_limit();

void vvctre_settings_set_use_custom_cpu_ticks(bool value);
bool vvctre_settings_get_use_custom_cpu_ticks();

void vvctre_settings_set_custom_cpu_ticks(u64 value);
u64 vvctre_settings_get_custom_cpu_ticks();

void vvctre_settings_set_cpu_clock_percentage(u32 value);
u32 vvctre_settings_get_cpu_clock_percentage();

// Audio Settings
void vvctre_settings_set_enable_dsp_lle(bool value);
bool vvctre_settings_get_enable_dsp_lle();

void vvctre_settings_set_enable_dsp_lle_multithread(bool value);
bool vvctre_settings_get_enable_dsp_lle_multithread();

void vvctre_settings_set_audio_volume(float value);
float vvctre_settings_get_audio_volume();

void vvctre_settings_set_audio_sink_id(const char* value);
const char* vvctre_settings_get_audio_sink_id();

void vvctre_settings_set_audio_device_id(const char* value);
const char* vvctre_settings_get_audio_device_id();

void vvctre_settings_set_microphone_input_type(int value);
int vvctre_settings_get_microphone_input_type();

void vvctre_settings_set_microphone_device(const char* value);
const char* vvctre_settings_get_microphone_device();

// Camera Settings
void vvctre_settings_set_camera_engine(int index, const char* value);
const char* vvctre_settings_get_camera_engine(int index);

void vvctre_settings_set_camera_parameter(int index, const char* value);
const char* vvctre_settings_get_camera_parameter(int index);

void vvctre_settings_set_camera_flip(int index, int value);
int vvctre_settings_get_camera_flip(int index);

// Graphics Settings
void vvctre_settings_set_use_hardware_renderer(bool value);
bool vvctre_settings_get_use_hardware_renderer();

void vvctre_settings_set_use_hardware_shader(bool value);
bool vvctre_settings_get_use_hardware_shader();

void vvctre_settings_set_hardware_shader_accurate_multiplication(bool value);
bool vvctre_settings_get_hardware_shader_accurate_multiplication();

void vvctre_settings_set_use_shader_jit(bool value);
bool vvctre_settings_get_use_shader_jit();

void vvctre_settings_set_enable_vsync(bool value);
bool vvctre_settings_get_enable_vsync();

void vvctre_settings_set_dump_textures(bool value);
bool vvctre_settings_get_dump_textures();

void vvctre_settings_set_custom_textures(bool value);
bool vvctre_settings_get_custom_textures();

void vvctre_settings_set_preload_textures(bool value);
bool vvctre_settings_get_preload_textures();

void vvctre_settings_set_enable_linear_filtering(bool value);
bool vvctre_settings_get_enable_linear_filtering();

void vvctre_settings_set_sharper_distant_objects(bool value);
bool vvctre_settings_get_sharper_distant_objects();

void vvctre_settings_set_resolution(u16 value);
u16 vvctre_settings_get_resolution();

void vvctre_settings_set_background_color_red(float value);
float vvctre_settings_get_background_color_red();

void vvctre_settings_set_background_color_green(float value);
float vvctre_settings_get_background_color_green();

void vvctre_settings_set_background_color_blue(float value);
float vvctre_settings_get_background_color_blue();

void vvctre_settings_set_post_processing_shader(const char* value);
const char* vvctre_settings_get_post_processing_shader();

void vvctre_settings_set_texture_filter(const char* value);
const char* vvctre_settings_get_texture_filter();

void vvctre_settings_set_render_3d(int value);
int vvctre_settings_get_render_3d();

void vvctre_settings_set_factor_3d(u8 value);
u8 vvctre_settings_get_factor_3d();

// Controls Settings
void vvctre_settings_set_button(int index, const char* params);
const char* vvctre_settings_get_button(int index);

void vvctre_settings_set_analog(int index, const char* params);
const char* vvctre_settings_get_analog(int index);

void vvctre_settings_set_motion_device(const char* params);
const char* vvctre_settings_get_motion_device();

void vvctre_settings_set_touch_device(const char* params);
const char* vvctre_settings_get_touch_device();

void vvctre_settings_set_cemuhookudp_address(const char* value);
const char* vvctre_settings_get_cemuhookudp_address();

void vvctre_settings_set_cemuhookudp_port(u16 value);
u16 vvctre_settings_get_cemuhookudp_port();

void vvctre_settings_set_cemuhookudp_pad_index(u8 value);
u8 vvctre_settings_get_cemuhookudp_pad_index();

// Layout Settings
void vvctre_settings_set_layout(int value);
int vvctre_settings_get_layout();

void vvctre_settings_set_swap_screens(bool value);
bool vvctre_settings_get_swap_screens();

void vvctre_settings_set_upright_screens(bool value);
bool vvctre_settings_get_upright_screens();

void vvctre_settings_set_use_custom_layout(bool value);
bool vvctre_settings_get_use_custom_layout();

void vvctre_settings_set_custom_layout_top_left(u16 value);
u16 vvctre_settings_get_custom_layout_top_left();

void vvctre_settings_set_custom_layout_top_top(u16 value);
u16 vvctre_settings_get_custom_layout_top_top();

void vvctre_settings_set_custom_layout_top_right(u16 value);
u16 vvctre_settings_get_custom_layout_top_right();

void vvctre_settings_set_custom_layout_top_bottom(u16 value);
u16 vvctre_settings_get_custom_layout_top_bottom();

void vvctre_settings_set_custom_layout_bottom_left(u16 value);
u16 vvctre_settings_get_custom_layout_bottom_left();

void vvctre_settings_set_custom_layout_bottom_top(u16 value);
u16 vvctre_settings_get_custom_layout_bottom_top();

void vvctre_settings_set_custom_layout_bottom_right(u16 value);
u16 vvctre_settings_get_custom_layout_bottom_right();

void vvctre_settings_set_custom_layout_bottom_bottom(u16 value);
u16 vvctre_settings_get_custom_layout_bottom_bottom();

// LLE Modules Settings
void vvctre_settings_set_use_lle_module(const char* name, bool value);
bool vvctre_settings_get_use_lle_module(const char* name);

// Other
const char* vvctre_get_version();
bool vvctre_emulation_running(void* core);

void vvctre_set_play_coins(u16 value);
u16 vvctre_get_play_coins();
