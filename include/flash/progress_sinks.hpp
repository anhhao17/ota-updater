#pragma once

#include "flash/progress.hpp"

#include <string>

namespace flash {

class FileProgressSink final : public IProgress {
public:
    explicit FileProgressSink(std::string path);

    void OnProgress(const ProgressEvent& e) override;

private:
    std::string path_;
};

class ConsoleProgressSink final : public IProgress {
public:
    ConsoleProgressSink() = default;

    void OnProgress(const ProgressEvent& e) override;
};

bool IsProgressLineActive();
void ClearProgressLine();

} // namespace flash
