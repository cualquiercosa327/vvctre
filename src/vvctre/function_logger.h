// Copyright 2020 vvctre project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/logging/backend.h"

namespace Log {

class FunctionLogger : public Log::Backend {
public:
    using Function = void (*)(const char* log);

    explicit FunctionLogger(Function function, std::string name);

    const char* GetName() const override;
    void Write(const Entry& entry) override;

private:
    Function function;
    std::string name;
};

} // namespace Log
