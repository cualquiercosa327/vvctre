// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <fmt/format.h>
#include "common/common_types.h"

namespace Log {

// trims up to and including the last of ../, ..\, src/, src\ in a string
constexpr const char* TrimSourcePath(std::string_view source) {
    const auto rfind = [source](const std::string_view match) {
        return source.rfind(match) == source.npos ? 0 : (source.rfind(match) + match.size());
    };
    auto idx = std::max({rfind("src/"), rfind("src\\"), rfind("../"), rfind("..\\")});
    return source.data() + idx;
}

/// Specifies the severity or level of detail of the log message.
enum class Level : u8 {
    Trace,    ///< Extremely detailed and repetitive debugging information that is likely to
              ///< pollute logs.
    Debug,    ///< Less detailed debugging information.
    Info,     ///< Status information from important points during execution.
    Warning,  ///< Minor or potential problems found during execution of a task.
    Error,    ///< Major problems found during execution of a task that prevent it from being
              ///< completed.
    Critical, ///< Major problems during execution that threaten the stability of the entire
              ///< application.

    Count ///< Total number of logging levels
};

typedef u8 ClassType;

/**
 * Specifies the sub-system that generated the log message.
 *
 * @note If you add a new entry here, also add a corresponding one to `ALL_LOG_CLASSES` in
 * backend.cpp.
 */
enum class Class : ClassType {
    Log,               ///< Messages about the log system itself
    Common,            ///< Library routines
    Common_Filesystem, ///< Filesystem interface library
    Common_Memory,     ///< Memory mapping and management functions
    Core,              ///< LLE emulation core
    Core_ARM11,        ///< ARM11 CPU core
    Core_Timing,       ///< CoreTiming functions
    Core_Cheats,       ///< Cheat functions
    Settings,          ///< Settings
    Debug,             ///< Debugging tools
    Debug_Emulated,    ///< Debug messages from the emulated programs
    Debug_GDBStub,     ///< GDB Stub
    Kernel,            ///< The HLE implementation of the CTR kernel
    Kernel_SVC,        ///< Kernel system calls
    Applet,            ///< HLE implementation of system applets. Each applet
                       ///< should have its own subclass.
    Applet_SWKBD,      ///< The Software Keyboard applet
    Service,           ///< HLE implementation of system services. Each major service
                       ///< should have its own subclass.
    Service_SRV,       ///< The SRV (Service Directory) implementation
    Service_FRD,       ///< The FRD (Friends) service
    Service_FS,        ///< The FS (Filesystem) service implementation
    Service_ERR,       ///< The ERR (Error) port implementation
    Service_APT,       ///< The APT (Applets) service
    Service_BOSS,      ///< The BOSS (SpotPass) service
    Service_GSP,       ///< The GSP (GPU control) service
    Service_AC,        ///< The AC (WiFi status) service
    Service_AM,        ///< The AM (Application manager) service
    Service_PTM,       ///< The PTM (Power status & misc.) service
    Service_LDR,       ///< The LDR (3ds dll loader) service
    Service_MIC,       ///< The MIC (Microphone) service
    Service_NDM,       ///< The NDM (Network daemon manager) service
    Service_NFC,       ///< The NFC service
    Service_NIM,       ///< The NIM (Network interface manager) service
    Service_NS,        ///< The NS (Nintendo User Interface Shell) service
    Service_NWM,       ///< The NWM (Network wlan manager) service
    Service_CAM,       ///< The CAM (Camera) service
    Service_CECD,      ///< The CECD (StreetPass) service
    Service_CFG,       ///< The CFG (Configuration) service
    Service_CSND,      ///< The CSND (CWAV format process) service
    Service_DSP,       ///< The DSP (DSP control) service
    Service_DLP,       ///< The DLP (Download Play) service
    Service_HID,       ///< The HID (Human interface device) service
    Service_HTTP,      ///< The HTTP service
    Service_SOC,       ///< The SOC (Socket) service
    Service_IR,        ///< The IR service
    Service_Y2R,       ///< The Y2R (YUV to RGB conversion) service
    Service_PS,        ///< The PS (Process) service
    HW,                ///< Low-level hardware emulation
    HW_Memory,         ///< Memory-map and address translation
    HW_LCD,            ///< LCD register emulation
    HW_GPU,            ///< GPU control emulation
    HW_AES,            ///< AES engine emulation
    Frontend,          ///< Emulator UI
    Render,            ///< Emulator video output and hardware acceleration
    Render_Software,   ///< Software renderer backend
    Render_OpenGL,     ///< OpenGL backend
    Audio,             ///< Audio emulation
    Audio_DSP,         ///< The HLE implementation of the DSP
    Audio_Sink,        ///< Emulator audio output backend
    Loader,            ///< ROM loader
    Input,             ///< Input emulation
    Movie,             ///< Movie (Input Recording) Playback
    RPC_Server,        ///< RPC server
    Count              ///< Total number of logging classes
};

/// Logs a message to the global logger, using fmt
void FmtLogMessageImpl(Class log_class, Level log_level, const char* filename,
                       unsigned int line_num, const char* function, const char* format,
                       const fmt::format_args& args);

template <typename... Args>
void FmtLogMessage(Class log_class, Level log_level, const char* filename, unsigned int line_num,
                   const char* function, const char* format, const Args&... args) {
    FmtLogMessageImpl(log_class, log_level, filename, line_num, function, format,
                      fmt::make_format_args(args...));
}

} // namespace Log

// Define the fmt lib macros
#define LOG_GENERIC(log_class, log_level, ...)                                                     \
    ::Log::FmtLogMessage(log_class, log_level, ::Log::TrimSourcePath(__FILE__), __LINE__,          \
                         __func__, __VA_ARGS__)

#ifdef _DEBUG
#define LOG_TRACE(log_class, ...)                                                                  \
    ::Log::FmtLogMessage(::Log::Class::log_class, ::Log::Level::Trace,                             \
                         ::Log::TrimSourcePath(__FILE__), __LINE__, __func__, __VA_ARGS__)
#else
#define LOG_TRACE(log_class, fmt, ...) (void(0))
#endif

#define LOG_DEBUG(log_class, ...)                                                                  \
    ::Log::FmtLogMessage(::Log::Class::log_class, ::Log::Level::Debug,                             \
                         ::Log::TrimSourcePath(__FILE__), __LINE__, __func__, __VA_ARGS__)
#define LOG_INFO(log_class, ...)                                                                   \
    ::Log::FmtLogMessage(::Log::Class::log_class, ::Log::Level::Info,                              \
                         ::Log::TrimSourcePath(__FILE__), __LINE__, __func__, __VA_ARGS__)
#define LOG_WARNING(log_class, ...)                                                                \
    ::Log::FmtLogMessage(::Log::Class::log_class, ::Log::Level::Warning,                           \
                         ::Log::TrimSourcePath(__FILE__), __LINE__, __func__, __VA_ARGS__)
#define LOG_ERROR(log_class, ...)                                                                  \
    ::Log::FmtLogMessage(::Log::Class::log_class, ::Log::Level::Error,                             \
                         ::Log::TrimSourcePath(__FILE__), __LINE__, __func__, __VA_ARGS__)
#define LOG_CRITICAL(log_class, ...)                                                               \
    ::Log::FmtLogMessage(::Log::Class::log_class, ::Log::Level::Critical,                          \
                         ::Log::TrimSourcePath(__FILE__), __LINE__, __func__, __VA_ARGS__)
