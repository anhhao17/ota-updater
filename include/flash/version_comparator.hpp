#pragma once

#include <string>

namespace flash {

class VersionComparator {
public:
    static int Compare(const std::string& lhs, const std::string& rhs);
};

} // namespace flash
