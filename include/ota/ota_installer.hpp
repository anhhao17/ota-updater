#pragma once

#include "util/result.hpp"
#include "ota/update_module.hpp"
#include <string>

namespace flash {

class OtaInstaller {
public:
    OtaInstaller();
    explicit OtaInstaller(UpdateModule update_module);

    void SetProgressSink(IProgress* sink) { progress_sink_ = sink; }

    Result Run(const std::string& input_path);

private:
    UpdateModule update_module_;
    IProgress* progress_sink_ = nullptr;
};

} // namespace flash
