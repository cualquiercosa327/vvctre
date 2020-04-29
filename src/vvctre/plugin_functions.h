// Copyright 2020 vvctre project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

// File, Emulation
VVCTRE_PLUGIN_FUNCTION void vvctre_load_file(void* core, const char* path);
VVCTRE_PLUGIN_FUNCTION void vvctre_restart(void* core);
VVCTRE_PLUGIN_FUNCTION void vvctre_set_paused(void* plugin_manager, bool paused);

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

// Other
VVCTRE_PLUGIN_FUNCTION const char* vvctre_get_version();

// GUI
VVCTRE_PLUGIN_FUNCTION void vvctre_gui_text(const char* text);
VVCTRE_PLUGIN_FUNCTION bool vvctre_gui_button(const char* text);

VVCTRE_PLUGIN_FUNCTION bool vvctre_gui_begin(const char* name);
VVCTRE_PLUGIN_FUNCTION void vvctre_gui_end();

VVCTRE_PLUGIN_FUNCTION bool vvctre_gui_begin_menu(const char* name);
VVCTRE_PLUGIN_FUNCTION void vvctre_gui_end_menu();

VVCTRE_PLUGIN_FUNCTION bool vvctre_gui_menu_item(const char* name);

// TODO:
// - Settings
// - TAS
// - Remote Control
// - Memory
// - Install CIA
// - Load/remove amiibo
// - Reload camera images
// - Frametime recording
// - HLE service function hooks
// - Custom logging backends
