#include "ota/staging_verifier.hpp"

#include "crypto/sha256.hpp"
#include "io/file_reader.hpp"

#include <cctype>
#include <cerrno>
#include <algorithm>
#include <fcntl.h>
#include <string>
#include <unistd.h>
#include <vector>

namespace flash {

namespace {

std::string NormalizeHex(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return s;
}

Result WriteAllToFd(int fd, std::span<const std::uint8_t> data) {
    size_t off = 0;
    while (off < data.size()) {
        const ssize_t n = ::write(fd, data.data() + off, data.size() - off);
        if (n < 0)
            return Result::Fail(errno, "write failed");
        off += static_cast<size_t>(n);
    }
    return Result::Ok();
}

} // namespace

Result TempFile::Create(TempFile& out) {
    char tmpl[] = "/tmp/ota-entry-XXXXXX";
    const int fd = ::mkstemp(tmpl);
    if (fd < 0)
        return Result::Fail(errno, "mkstemp failed");
    out.fd_.Reset(fd);
    out.path_ = tmpl;
    return Result::Ok();
}

TempFile::TempFile() = default;
TempFile::TempFile(TempFile&& other) noexcept { *this = std::move(other); }
TempFile& TempFile::operator=(TempFile&& other) noexcept {
    if (this != &other) {
        Cleanup();
        fd_ = std::move(other.fd_);
        path_ = std::move(other.path_);
    }
    return *this;
}
TempFile::~TempFile() { Cleanup(); }

int TempFile::GetFd() const { return fd_.Get(); }
const std::string& TempFile::Path() const { return path_; }

void TempFile::Close() { fd_.Close(); }

void TempFile::Cleanup() {
    Close();
    if (!path_.empty()) {
        ::unlink(path_.c_str());
        path_.clear();
    }
}

Result OtaEntryStager::StageAndVerify(std::unique_ptr<IReader>& entry_reader,
                                      const std::string& expected_sha256,
                                      StagedEntry& out) const {
    if (expected_sha256.empty())
        return Result::Fail(-1, "expected sha256 is empty");

    TempFile tmp;
    auto cr = TempFile::Create(tmp);
    if (!cr.is_ok())
        return cr;

    Sha256Hasher hasher;
    std::vector<std::uint8_t> buf(64 * 1024);

    while (true) {
        const ssize_t n = entry_reader->Read(std::span<std::uint8_t>(buf.data(), buf.size()));
        if (n < 0)
            return Result::Fail(-1, "read failed while staging");
        if (n == 0)
            break;

        const auto chunk = std::span<const std::uint8_t>(buf.data(), static_cast<size_t>(n));
        hasher.Update(chunk);
        auto wr = WriteAllToFd(tmp.GetFd(), chunk);
        if (!wr.is_ok())
            return wr;
    }

    (void)::fsync(tmp.GetFd());
    tmp.Close();

    const std::string actual = hasher.FinalHex();
    if (actual.empty())
        return Result::Fail(-1, "sha256 compute failed");
    if (NormalizeHex(actual) != NormalizeHex(expected_sha256)) {
        return Result::Fail(-1,
                            "sha256 mismatch: expected=" + expected_sha256 + " actual=" + actual);
    }

    auto file_reader = std::make_unique<FileOrStdinReader>();
    auto orr = FileOrStdinReader::Open(tmp.Path(), *file_reader);
    if (!orr.ok)
        return Result::Fail(-1, orr.msg);

    out.temp = std::move(tmp);
    out.reader = std::move(file_reader);
    return Result::Ok();
}

} // namespace flash
