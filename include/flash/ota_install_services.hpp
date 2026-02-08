#pragma once

#include "flash/file_reader.hpp"
#include "flash/manifest.hpp"
#include "flash/ota_bundle_reader.hpp"
#include "flash/progress.hpp"
#include "flash/result.hpp"
#include "flash/update_module.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>

namespace flash {

class ComponentIndex {
public:
    explicit ComponentIndex(const Manifest& manifest);

    const Component* Find(std::string_view normalized_entry_name) const;
    bool Contains(std::string_view normalized_entry_name) const;

private:
    std::unordered_map<std::string, const Component*> by_filename_;
};

class ManifestLoader {
public:
    static Result LoadFromFirstBundleEntry(OtaTarBundleReader& bundle, Manifest& out_manifest);
};

class BundlePreScanner {
public:
    static std::uint64_t ComputeOverallTotalFromFile(const std::string& input_path,
                                                     const ComponentIndex& component_index);
};

class InstallCoordinator {
public:
    explicit InstallCoordinator(UpdateModule& update_module, IProgress* progress_sink = nullptr);

    Result InstallMatchingEntries(OtaTarBundleReader& bundle,
                                  const ComponentIndex& component_index,
                                  std::uint64_t overall_total);

private:
    static UpdateModule::Options BuildOptions(std::uint64_t component_total_bytes,
                                              std::uint64_t overall_total_bytes,
                                              std::uint64_t overall_done_base_bytes,
                                              IProgress* progress_sink);

    UpdateModule& update_module_;
    IProgress* progress_sink_ = nullptr;
};

} // namespace flash
