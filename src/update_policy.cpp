#include "flash/update_policy.hpp"

#include "flash/version_comparator.hpp"

namespace flash {

bool UpdatePolicy::ShouldUpdate(const Component& comp,
                                const Manifest& manifest,
                                const std::string& current_version) {
    if (manifest.force_all || comp.force) return true;
    return VersionComparator::Compare(comp.version, current_version) > 0;
}

} // namespace flash
