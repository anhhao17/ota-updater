#include "util/manifest.hpp"

#include "util/manifest_parser.hpp"
#include "util/update_policy.hpp"
#include "util/version_comparator.hpp"

namespace flash {

std::expected<Manifest, std::string> ManifestHandler::Parse(const std::string& jsonInput) {
    ManifestParser parser;
    return parser.Parse(jsonInput);
}

int ManifestHandler::CompareVersions(const std::string& v1, const std::string& v2) {
    return VersionComparator::Compare(v1, v2);
}

bool ManifestHandler::ShouldUpdate(const Component& comp,
                                   const Manifest& manifest,
                                   const std::string& currentVersion) {
    return UpdatePolicy::ShouldUpdate(comp, manifest, currentVersion);
}

} // namespace flash
