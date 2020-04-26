// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <type_traits>
#include "common/assert.h"
#include "common/bit_field.h"
#include "common/common_funcs.h"
#include "common/common_types.h"

namespace Memory {
class MemorySystem;
} // namespace Memory

namespace GPU {

// TODO(xperia64): This should be defined by the number of
// ARM11 cycles per vblank interval once that value is measured
// and not the other way around
constexpr double SCREEN_REFRESH_RATE = 59.833997376556916;

// Returns index corresponding to the Regs member labeled by field_name
#define GPU_REG_INDEX(field_name) (offsetof(GPU::Regs, field_name) / sizeof(u32))

// MMIO region 0x1EFxxxxx
struct Regs {

// helper macro to make sure the defined structures are of the expected size.
#define ASSERT_MEMBER_SIZE(name, size_in_bytes)                                                    \
    static_assert(sizeof(name) == size_in_bytes,                                                   \
                  "Structure size and register block length don't match")

    // Components are laid out in reverse byte order, most significant bits first.
    enum class PixelFormat : u32 {
        RGBA8 = 0,
        RGB8 = 1,
        RGB565 = 2,
        RGB5A1 = 3,
        RGBA4 = 4,
    };

    /**
     * Returns the number of bytes per pixel.
     */
    static int BytesPerPixel(PixelFormat format) {
        switch (format) {
        case PixelFormat::RGBA8:
            return 4;
        case PixelFormat::RGB8:
            return 3;
        case PixelFormat::RGB565:
        case PixelFormat::RGB5A1:
        case PixelFormat::RGBA4:
            return 2;
        }

        UNREACHABLE();
    }

    INSERT_PADDING_WORDS(0x4);

    struct MemoryFillConfig {
        u32 address_start;
        u32 address_end;

        union {
            u32 value_32bit;

            BitField<0, 16, u32> value_16bit;

            // TODO: Verify component order
            BitField<0, 8, u32> value_24bit_r;
            BitField<8, 8, u32> value_24bit_g;
            BitField<16, 8, u32> value_24bit_b;
        };

        union {
            u32 control;

            // Setting this field to 1 triggers the memory fill.
            // This field also acts as a status flag, and gets reset to 0 upon completion.
            BitField<0, 1, u32> trigger;

            // Set to 1 upon completion.
            BitField<1, 1, u32> finished;

            // If both of these bits are unset, then it will fill the memory with a 16 bit value
            // 1: fill with 24-bit wide values
            BitField<8, 1, u32> fill_24bit;
            // 1: fill with 32-bit wide values
            BitField<9, 1, u32> fill_32bit;
        };

        inline u32 GetStartAddress() const {
            return DecodeAddressRegister(address_start);
        }

        inline u32 GetEndAddress() const {
            return DecodeAddressRegister(address_end);
        }
    } memory_fill_config[2];
    ASSERT_MEMBER_SIZE(memory_fill_config[0], 0x10);

    INSERT_PADDING_WORDS(0x10b);

    struct FramebufferConfig {
        union {
            u32 size;

            BitField<0, 16, u32> width;
            BitField<16, 16, u32> height;
        };

        INSERT_PADDING_WORDS(0x2);

        u32 address_left1;
        u32 address_left2;

        union {
            u32 format;

            BitField<0, 3, PixelFormat> color_format;
        };

        INSERT_PADDING_WORDS(0x1);

        union {
            u32 active_fb;

            // 0: Use parameters ending with "1"
            // 1: Use parameters ending with "2"
            BitField<0, 1, u32> second_fb_active;
        };

        INSERT_PADDING_WORDS(0x5);

        // Distance between two pixel rows, in bytes
        u32 stride;

        u32 address_right1;
        u32 address_right2;

        INSERT_PADDING_WORDS(0x30);
    } framebuffer_config[2];
    ASSERT_MEMBER_SIZE(framebuffer_config[0], 0x100);

    INSERT_PADDING_WORDS(0x169);

    struct DisplayTransferConfig {
        u32 input_address;
        u32 output_address;

        inline u32 GetPhysicalInputAddress() const {
            return DecodeAddressRegister(input_address);
        }

        inline u32 GetPhysicalOutputAddress() const {
            return DecodeAddressRegister(output_address);
        }

        union {
            u32 output_size;

            BitField<0, 16, u32> output_width;
            BitField<16, 16, u32> output_height;
        };

        union {
            u32 input_size;

            BitField<0, 16, u32> input_width;
            BitField<16, 16, u32> input_height;
        };

        enum ScalingMode : u32 {
            NoScale = 0, // Doesn't scale the image
            ScaleX = 1,  // Downscales the image in half in the X axis and applies a box filter
            ScaleXY =
                2, // Downscales the image in half in both the X and Y axes and applies a box filter
        };

        union {
            u32 flags;

            BitField<0, 1, u32> flip_vertically; // flips input data vertically
            BitField<1, 1, u32> input_linear;    // Converts from linear to tiled format
            BitField<2, 1, u32> crop_input_lines;
            BitField<3, 1, u32> is_texture_copy; // Copies the data without performing any
                                                 // processing and respecting texture copy fields
            BitField<5, 1, u32> dont_swizzle;
            BitField<8, 3, PixelFormat> input_format;
            BitField<12, 3, PixelFormat> output_format;
            /// Uses some kind of 32x32 block swizzling mode, instead of the usual 8x8 one.
            BitField<16, 1, u32> block_32;        // TODO(yuriks): unimplemented
            BitField<24, 2, ScalingMode> scaling; // Determines the scaling mode of the transfer
        };

        INSERT_PADDING_WORDS(0x1);

        // it seems that writing to this field triggers the display transfer
        u32 trigger;

        INSERT_PADDING_WORDS(0x1);

        struct {
            u32 size; // The lower 4 bits are ignored

            union {
                u32 input_size;

                BitField<0, 16, u32> input_width;
                BitField<16, 16, u32> input_gap;
            };

            union {
                u32 output_size;

                BitField<0, 16, u32> output_width;
                BitField<16, 16, u32> output_gap;
            };
        } texture_copy;
    } display_transfer_config;
    ASSERT_MEMBER_SIZE(display_transfer_config, 0x2c);

    INSERT_PADDING_WORDS(0x32D);

    struct {
        // command list size (in bytes)
        u32 size;

        INSERT_PADDING_WORDS(0x1);

        // command list address
        u32 address;

        INSERT_PADDING_WORDS(0x1);

        // it seems that writing to this field triggers command list processing
        u32 trigger;

        inline u32 GetPhysicalAddress() const {
            return DecodeAddressRegister(address);
        }
    } command_processor_config;
    ASSERT_MEMBER_SIZE(command_processor_config, 0x14);

    INSERT_PADDING_WORDS(0x9c3);

    static constexpr std::size_t NumIds() {
        return sizeof(Regs) / sizeof(u32);
    }

    const u32& operator[](int index) const {
        const u32* content = reinterpret_cast<const u32*>(this);
        return content[index];
    }

    u32& operator[](int index) {
        u32* content = reinterpret_cast<u32*>(this);
        return content[index];
    }

#undef ASSERT_MEMBER_SIZE

private:
    /*
     * Most physical addresses which GPU registers refer to are 8-byte aligned.
     * This function should be used to get the address from a raw register value.
     */
    static inline u32 DecodeAddressRegister(u32 register_value) {
        return register_value * 8;
    }
};
static_assert(std::is_standard_layout<Regs>::value, "Structure does not use standard layout");

#define ASSERT_REG_POSITION(field_name, position)                                                  \
    static_assert(offsetof(Regs, field_name) == position * 4,                                      \
                  "Field " #field_name " has invalid position")

ASSERT_REG_POSITION(memory_fill_config[0], 0x00004);
ASSERT_REG_POSITION(memory_fill_config[1], 0x00008);
ASSERT_REG_POSITION(framebuffer_config[0], 0x00117);
ASSERT_REG_POSITION(framebuffer_config[1], 0x00157);
ASSERT_REG_POSITION(display_transfer_config, 0x00300);
ASSERT_REG_POSITION(command_processor_config, 0x00638);

#undef ASSERT_REG_POSITION

// The total number of registers is chosen arbitrarily, but let's make sure it's not some odd value
// anyway.
static_assert(sizeof(Regs) == 0x1000 * sizeof(u32), "Invalid total size of register set");

extern Regs g_regs;

template <typename T>
void Read(T& var, const u32 addr);

template <typename T>
void Write(u32 addr, const T data);

/// Initialize hardware
void Init(Memory::MemorySystem& memory);

/// Shutdown hardware
void Shutdown();

} // namespace GPU
