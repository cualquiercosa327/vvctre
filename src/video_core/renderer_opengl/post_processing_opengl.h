// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <vector>

namespace OpenGL {

// Returns a vector of the names of the shaders available in the
// "shaders" directory in vvctre's data directory
std::vector<std::string> GetPostProcessingShaderList(const bool single_screen);

// Returns the shader code for the shader named "shader_name"
// with the appropriate header prepended to it
// If the shader cannot be loaded, an empty string is returned
std::string GetPostProcessingShaderCode(const bool single_screen, const std::string shader_name);

} // namespace OpenGL
