#include "util/manifest_parser.hpp"

#include <cstring>
#include <nlohmann/json.hpp>

namespace flash {

using json = nlohmann::json;

namespace {

bool StartsWith(const std::string& s, const char* prefix) {
    const size_t n = std::strlen(prefix);
    return s.size() >= n && s.compare(0, n, prefix) == 0;
}

std::expected<std::vector<Component>, std::string> ParseComponentsArray(const json& arr) {
    if (!arr.is_array()) {
        return std::unexpected("'components' must be an array");
    }

    std::vector<Component> out;
    out.reserve(arr.size());

    for (const auto& item : arr) {
        Component c;
        c.name = item.value("name", "");
        c.type = item.value("type", "");
        c.filename = item.value("filename", "");
        c.size = item.value("size", 0ULL);
        c.sha256 = item.value("sha256", "");
        c.version = item.value("version", "0.0.0");
        c.force = item.value("force", false);
        c.install_to = item.value("install_to", "");
        c.path = item.value("path", "");
        c.permissions = item.value("permissions", "");
        c.create_destination = item.value("create-destination", false);
        out.push_back(c);
    }

    return out;
}

} // namespace

std::expected<Manifest, std::string> ManifestParser::Parse(const std::string& json_input) const {
    try {
        if (json_input.find_first_not_of(" \t\n\r") == std::string::npos) {
            return std::unexpected("Empty input");
        }

        auto j = json::parse(json_input);
        if (!j.is_object()) {
            return std::unexpected("JSON root must be an object");
        }

        Manifest m;
        m.version = j.value("version", "0.0.0");
        m.hw_compatibility = j.value("hw_compatibility", "");
        m.force_all = j.value("force_all", false);

        if (j.contains("components")) {
            auto parsed = ParseComponentsArray(j["components"]);
            if (!parsed)
                return std::unexpected(parsed.error());
            m.components = std::move(*parsed);
        }

        for (const auto& [key, val] : j.items()) {
            if (!StartsWith(key, "slot-"))
                continue;
            if (!val.is_object()) {
                return std::unexpected("slot section must be object: " + key);
            }
            if (!val.contains("components")) {
                return std::unexpected("slot section missing components: " + key);
            }
            auto parsed = ParseComponentsArray(val["components"]);
            if (!parsed)
                return std::unexpected(parsed.error());
            m.slot_components.emplace(key, std::move(*parsed));
        }

        return m;
    } catch (const json::parse_error& e) {
        return std::unexpected(std::string("Syntax Error: ") + e.what());
    } catch (const std::exception& e) {
        return std::unexpected(std::string("Internal Error: ") + e.what());
    }
}

} // namespace flash
