#include "ota/ota_installer.hpp"

#include "io/file_reader.hpp"
#include "ota/ota_install_services.hpp"
#include "util/device_config.hpp"
#include "util/logger.hpp"
#include "util/manifest_selector.hpp"

namespace {
constexpr const char kConfFile[] = "/run/ota-updater/ota.conf";
} // namespace

namespace flash {

OtaInstaller::OtaInstaller() = default;

OtaInstaller::OtaInstaller(UpdateModule update_module) : update_module_(std::move(update_module)) {}

Result OtaInstaller::Run(const std::string& input_path) {
    FileOrStdinReader input;
    auto open_result = FileOrStdinReader::Open(input_path.c_str(), input);
    if (!open_result.ok)
        return Result::Fail(-1, open_result.msg);

    OtaTarBundleReader bundle;
    auto bundle_result = bundle.Open(input);
    if (!bundle_result.is_ok())
        return bundle_result;

    Manifest manifest{};
    auto manifest_result = ManifestLoader::LoadFromFirstBundleEntry(bundle, manifest);
    if (!manifest_result.is_ok())
        return manifest_result;

    DeviceConfig device_cfg;
    auto cfg_res = DeviceConfig::LoadFromFile(kConfFile, device_cfg);
    if (!cfg_res.is_ok())
        return cfg_res;

    Manifest selected{};
    auto sel_res = ManifestSelector::SelectForDevice(manifest, device_cfg, selected);
    if (!sel_res.is_ok())
        return sel_res;
    manifest = std::move(selected);

    LogInfo("Device config: slot=%s hw=%s",
            device_cfg.current_slot.c_str(),
            device_cfg.hw_compatibility.c_str());
    LogInfo("Selected components: %zu", manifest.components.size());

    const ComponentIndex component_index(manifest);

    const std::uint64_t overall_total =
        BundlePreScanner::ComputeOverallTotalFromFile(input_path, component_index);
    if (overall_total > 0) {
        LogInfo("OTA overall total (bundle bytes) = %llu", (unsigned long long)overall_total);
    } else {
        LogInfo("OTA overall total unknown (stdin or pre-scan failed)");
    }

    InstallCoordinator coordinator(update_module_, progress_sink_);
    auto install_result =
        coordinator.InstallMatchingEntries(bundle, component_index, overall_total);
    if (!install_result.is_ok())
        return install_result;

    LogInfo("OTA completed successfully");
    return Result::Ok();
}

} // namespace flash
