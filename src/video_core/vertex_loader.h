// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include "common/common_types.h"
#include "video_core/regs_pipeline.h"

namespace Pica {

namespace Shader {
struct AttributeBuffer;
} // namespace Shader

class VertexLoader {
public:
    VertexLoader() = default;
    explicit VertexLoader(const PipelineRegs& regs) {
        Setup(regs);
    }

    void Setup(const PipelineRegs& regs);
    void LoadVertex(u32 base_address, int index, int vertex, Shader::AttributeBuffer& input);

    int GetNumTotalAttributes() const {
        return num_total_attributes;
    }

private:
    std::array<u32, 16> vertex_attribute_sources;
    std::array<u32, 16> vertex_attribute_strides{};
    std::array<PipelineRegs::VertexAttributeFormat, 16> vertex_attribute_formats;
    std::array<u32, 16> vertex_attribute_elements{};
    std::array<bool, 16> vertex_attribute_is_default;
    int num_total_attributes = 0;
    bool is_setup = false;
};

} // namespace Pica
