#pragma once

#include "io/fd.hpp"
#include "io/io.hpp"
#include "util/result.hpp"

#include <memory>
#include <string>

namespace flash {

class Fd;

class TempFile {
public:
    static Result Create(TempFile& out);

    TempFile();
    TempFile(const TempFile&) = delete;
    TempFile& operator=(const TempFile&) = delete;
    TempFile(TempFile&& other) noexcept;
    TempFile& operator=(TempFile&& other) noexcept;
    ~TempFile();

    int GetFd() const;
    const std::string& Path() const;
    void Close();

private:
    void Cleanup();

    Fd fd_;
    std::string path_;
};

struct StagedEntry {
    TempFile temp;
    std::unique_ptr<IReader> reader;
};

class OtaEntryStager {
public:
    Result StageAndVerify(std::unique_ptr<IReader>& entry_reader,
                          const std::string& expected_sha256,
                          StagedEntry& out) const;
};

} // namespace flash
