// Copyright 2020 vvctre project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"

#define VVCTRE_PLUGIN_FUNCTION extern "C" __declspec(dllimport)

// File
VVCTRE_PLUGIN_FUNCTION void vvctre_load_file(void* core, const char* path);
VVCTRE_PLUGIN_FUNCTION bool vvctre_install_cia(const char* path);

VVCTRE_PLUGIN_FUNCTION void vvctre_load_amiibo(void* core, const char* path);
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

// Other
VVCTRE_PLUGIN_FUNCTION const char* vvctre_get_version();
VVCTRE_PLUGIN_FUNCTION bool vvctre_emulation_running(void* core);

// TODO: Settings
