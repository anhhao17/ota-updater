#include "util/version_comparator.hpp"

#include <charconv>
#include <ranges>
#include <string_view>

namespace flash {

int VersionComparator::Compare(const std::string& lhs, const std::string& rhs) {
    if (lhs == rhs)
        return 0;

    auto lhs_parts = lhs | std::views::split('.') |
                     std::views::transform([](auto&& rng) { return std::string_view(rng); });
    auto rhs_parts = rhs | std::views::split('.') |
                     std::views::transform([](auto&& rng) { return std::string_view(rng); });

    auto it_lhs = lhs_parts.begin();
    auto it_rhs = rhs_parts.begin();

    while (it_lhs != lhs_parts.end() || it_rhs != rhs_parts.end()) {
        int lhs_val = 0;
        int rhs_val = 0;

        if (it_lhs != lhs_parts.end()) {
            auto sv = *it_lhs;
            std::from_chars(sv.data(), sv.data() + sv.size(), lhs_val);
            ++it_lhs;
        }

        if (it_rhs != rhs_parts.end()) {
            auto sv = *it_rhs;
            std::from_chars(sv.data(), sv.data() + sv.size(), rhs_val);
            ++it_rhs;
        }

        if (lhs_val > rhs_val)
            return 1;
        if (lhs_val < rhs_val)
            return -1;
    }

    return 0;
}

} // namespace flash
