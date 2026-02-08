#include "ota/ota_install_services.hpp"

#include "ota/staging_verifier.hpp"
#include "util/logger.hpp"
#include "util/path_utils.hpp"

#include <memory>
#include <string>

namespace flash {

namespace {
const OtaEntryStager kEntryStager{};
} // namespace
ComponentIndex::ComponentIndex(const Manifest& manifest) {
    by_filename_.reserve(manifest.components.size());
    for (const auto& component : manifest.components) {
        if (!component.filename.empty()) {
            by_filename_.emplace(component.filename, &component);
        }
    }
}

const Component* ComponentIndex::Find(std::string_view normalized_entry_name) const {
    auto it = by_filename_.find(std::string(normalized_entry_name));
    if (it == by_filename_.end())
        return nullptr;
    return it->second;
}

bool ComponentIndex::Contains(std::string_view normalized_entry_name) const {
    return Find(normalized_entry_name) != nullptr;
}

Result ManifestLoader::LoadFromFirstBundleEntry(OtaTarBundleReader& bundle,
                                                Manifest& out_manifest) {
    bool eof = false;
    BundleEntryInfo entry{};

    auto next_result = bundle.Next(entry, eof);
    if (!next_result.is_ok())
        return next_result;
    if (eof)
        return Result::Fail(-1, "Empty ota.tar");

    const std::string entry_name = NormalizeTarPath(entry.name);
    if (entry_name != "manifest.json") {
        return Result::Fail(
            -1, "manifest.json must be the first entry in ota.tar (got: " + entry_name + ")");
    }

    std::string manifest_json;
    auto read_result = bundle.ReadCurrentToString(manifest_json);
    if (!read_result.is_ok())
        return read_result;

    auto parsed = ManifestHandler::Parse(manifest_json);
    if (!parsed)
        return Result::Fail(-1, "Manifest parse error: " + parsed.error());

    out_manifest = *parsed;
    LogInfo("Loaded manifest version=%s hw=%s components=%zu",
            out_manifest.version.c_str(),
            out_manifest.hw_compatibility.c_str(),
            out_manifest.components.size());
    return Result::Ok();
}

std::uint64_t BundlePreScanner::ComputeOverallTotalFromFile(const std::string& input_path,
                                                            const ComponentIndex& component_index) {
    if (input_path == "-")
        return 0;

    FileOrStdinReader input;
    auto open_result = FileOrStdinReader::Open(input_path.c_str(), input);
    if (!open_result.ok) {
        LogWarn("Pre-scan open failed, overall progress disabled: %s", open_result.msg.c_str());
        return 0;
    }

    OtaTarBundleReader bundle;
    auto bundle_result = bundle.Open(input);
    if (!bundle_result.is_ok()) {
        LogWarn("Pre-scan bundle open failed, overall progress disabled: %s",
                bundle_result.message().c_str());
        return 0;
    }

    bool eof = false;
    BundleEntryInfo entry{};
    std::uint64_t total = 0;

    auto manifest_next = bundle.Next(entry, eof);
    if (!manifest_next.is_ok() || eof)
        return 0;
    (void)bundle.SkipCurrent();

    while (true) {
        auto next_result = bundle.Next(entry, eof);
        if (!next_result.is_ok())
            return 0;
        if (eof)
            break;

        const std::string entry_name = NormalizeTarPath(entry.name);
        if (const Component* comp = component_index.Find(entry_name)) {
            const std::uint64_t comp_size = (comp->size > 0) ? comp->size : entry.size;
            total += comp_size;
        }

        auto skip_result = bundle.SkipCurrent();
        if (!skip_result.is_ok())
            return 0;
    }

    return total;
}

InstallCoordinator::InstallCoordinator(UpdateModule& update_module, IProgress* progress_sink)
    : update_module_(update_module), progress_sink_(progress_sink) {}

Result InstallCoordinator::InstallMatchingEntries(OtaTarBundleReader& bundle,
                                                  const ComponentIndex& component_index,
                                                  std::uint64_t overall_total) {
    std::uint64_t overall_done_base = 0;

    bool eof = false;
    BundleEntryInfo entry{};

    while (true) {
        auto next_result = bundle.Next(entry, eof);
        if (!next_result.is_ok())
            return next_result;
        if (eof)
            break;

        const std::string entry_name = NormalizeTarPath(entry.name);
        const Component* component = component_index.Find(entry_name);
        if (!component) {
            LogDebug("skip: %s", entry_name.c_str());
            auto skip_result = bundle.SkipCurrent();
            if (!skip_result.is_ok())
                return skip_result;
            continue;
        }

        LogInfo("Install: name=%s type=%s file=%s (entry=%llu bytes)",
                component->name.c_str(),
                component->type.c_str(),
                component->filename.c_str(),
                (unsigned long long)entry.size);

        std::unique_ptr<IReader> entry_reader;
        auto open_entry_result = bundle.OpenCurrentEntryReader(entry_reader);
        if (!open_entry_result.is_ok())
            return open_entry_result;

        const std::string expected_sha256 = component->sha256;
        StagedEntry staged;
        if (!expected_sha256.empty()) {
            auto vr = kEntryStager.StageAndVerify(entry_reader, expected_sha256, staged);
            if (!vr.is_ok()) {
                return Result::Fail(-1,
                                    "component '" + component->name +
                                        "' sha256 verify failed: " + vr.message());
            }
            entry_reader = std::move(staged.reader);
        }

        const std::uint64_t comp_total = component->size > 0 ? component->size : entry.size;
        auto update_result = update_module_.ExecuteComponent(
            *component,
            std::move(entry_reader),
            BuildOptions(comp_total, overall_total, overall_done_base, progress_sink_));
        if (!update_result.is_ok()) {
            return Result::Fail(
                -1, "component '" + component->name + "' failed: " + update_result.message());
        }

        overall_done_base += comp_total;

        auto skip_result = bundle.SkipCurrent();
        if (!skip_result.is_ok())
            return skip_result;
    }

    return Result::Ok();
}

UpdateModule::Options InstallCoordinator::BuildOptions(std::uint64_t component_total_bytes,
                                                       std::uint64_t overall_total_bytes,
                                                       std::uint64_t overall_done_base_bytes,
                                                       IProgress* progress_sink) {
    UpdateModule::Options options;
    options.progress = true;
    options.component_total_bytes = component_total_bytes;
    options.overall_total_bytes = overall_total_bytes;
    options.overall_done_base_bytes = overall_done_base_bytes;
    options.progress_sink = progress_sink;
    return options;
}

} // namespace flash
