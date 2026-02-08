#include "flash/ota_installer.hpp"

#include "flash/file_reader.hpp"
#include "flash/logger.hpp"
#include "flash/ota_install_services.hpp"

namespace flash {

OtaInstaller::OtaInstaller() = default;

OtaInstaller::OtaInstaller(UpdateModule update_module)
    : update_module_(std::move(update_module)) {}

Result OtaInstaller::Run(const std::string& input_path) {
    FileOrStdinReader input;
    auto open_result = FileOrStdinReader::Open(input_path.c_str(), input);
    if (!open_result.ok) return Result::Fail(-1, open_result.msg);

    OtaTarBundleReader bundle;
    auto bundle_result = bundle.Open(input);
    if (!bundle_result.is_ok()) return bundle_result;

    Manifest manifest{};
    auto manifest_result = ManifestLoader::LoadFromFirstBundleEntry(bundle, manifest);
    if (!manifest_result.is_ok()) return manifest_result;

    const ComponentIndex component_index(manifest);

    const std::uint64_t overall_total =
        BundlePreScanner::ComputeOverallTotalFromFile(input_path, component_index);
    if (overall_total > 0) {
        LogInfo("OTA overall total (bundle bytes) = %llu", (unsigned long long)overall_total);
    } else {
        LogInfo("OTA overall total unknown (stdin or pre-scan failed)");
    }

    InstallCoordinator coordinator(update_module_, progress_sink_);
    auto install_result = coordinator.InstallMatchingEntries(bundle, component_index, overall_total);
    if (!install_result.is_ok()) return install_result;

    LogInfo("OTA completed successfully");
    return Result::Ok();
}

} // namespace flash
