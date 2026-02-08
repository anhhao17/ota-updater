#include "ota/update_module.hpp"

#include "ota/component_installers.hpp"
#include "io/counting_reader.hpp"
#include "io/gzip_reader.hpp"
#include "util/logger.hpp"

#include <utility>

namespace flash {

namespace {

bool EndsWithGz(const std::string& s) {
    return s.size() >= 3 && s.compare(s.size() - 3, 3, ".gz") == 0;
}

} // namespace

UpdateModule::UpdateModule() : strategies_(CreateDefaultInstallerStrategies()) {}

UpdateModule::UpdateModule(std::vector<std::unique_ptr<IInstallerStrategy>> strategies)
    : strategies_(std::move(strategies)) {}

Result UpdateModule::ExecuteComponent(const Component& comp,
                                      std::unique_ptr<IReader> source,
                                      const Options& opt) {
    if (!source) return Result::Fail(-1, "Null source reader");

    const char* tag = comp.name.c_str();

    LogInfo("UpdateModule: name=%s type=%s file=%s",
            comp.name.c_str(), comp.type.c_str(), comp.filename.c_str());

    std::uint64_t in_read = 0;
    std::unique_ptr<IReader> effective_reader =
        std::make_unique<CountingReader>(std::move(source), &in_read);

    if (EndsWithGz(comp.filename)) {
        try {
            LogDebug("Wrapping GzipReader for %s", comp.filename.c_str());
            effective_reader = std::make_unique<GzipReader>(std::move(effective_reader));
        } catch (const std::exception& e) {
            return Result::Fail(-1, std::string("Gzip init failed: ") + e.what());
        }
    }

    for (const auto& strategy : strategies_) {
        if (strategy->Supports(comp)) {
            return strategy->Install(comp, *effective_reader, opt, tag, &in_read);
        }
    }

    return Result::Fail(-1, "Unsupported component type: " + comp.type);
}

Result UpdateModule::Execute(const Component& comp,
                             std::unique_ptr<IReader> source,
                             const Options& opt) {
    UpdateModule module;
    return module.ExecuteComponent(comp, std::move(source), opt);
}

} // namespace flash
