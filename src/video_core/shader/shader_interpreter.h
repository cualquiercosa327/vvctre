// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "video_core/shader/shader.h"

namespace Pica::Shader {

class InterpreterEngine final : public ShaderEngine {
public:
    void SetupBatch(ShaderSetup& setup, unsigned int entry_point) override;
    void Run(const ShaderSetup& setup, UnitState& state) const override;
};

} // namespace Pica::Shader
