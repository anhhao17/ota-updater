#pragma once

#include "io/io.hpp"
#include "util/manifest.hpp"
#include "ota/progress.hpp"
#include "util/result.hpp"

#include <cstdint>
#include <memory>
#include <vector>

namespace flash {

class UpdateModule {
public:
    struct Options {
        std::uint64_t fsync_interval_bytes = 1024 * 1024ULL;
        bool progress = true;
        std::uint64_t progress_interval_bytes = 4 * 1024 * 1024ULL;
        IProgress* progress_sink = nullptr;

        // For percent reporting (based on bundle entry bytes)
        std::uint64_t component_total_bytes = 0;     // set to BundleEntryInfo.size
        std::uint64_t overall_total_bytes = 0;       // 0 => unknown
        std::uint64_t overall_done_base_bytes = 0;   // sum completed entry sizes before current component
    };

    class IInstallerStrategy {
    public:
        virtual ~IInstallerStrategy() = default;
        virtual bool Supports(const Component& comp) const = 0;
        virtual Result Install(const Component& comp,
                               IReader& reader,
                               const Options& opt,
                               const char* tag,
                               const std::uint64_t* in_read) const = 0;
    };

    UpdateModule();
    explicit UpdateModule(std::vector<std::unique_ptr<IInstallerStrategy>> strategies);

    Result ExecuteComponent(const Component& comp, std::unique_ptr<IReader> source, const Options& opt);

    static Result Execute(const Component& comp, std::unique_ptr<IReader> source) {
        return Execute(comp, std::move(source), Options{});
    }

    static Result Execute(const Component& comp, std::unique_ptr<IReader> source, const Options& opt);

private:
    std::vector<std::unique_ptr<IInstallerStrategy>> strategies_;
};

} // namespace flash
