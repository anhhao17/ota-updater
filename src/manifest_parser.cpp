#include "flash/manifest_parser.hpp"

#include <nlohmann/json.hpp>

namespace flash {

using json = nlohmann::json;

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

        if (j.contains("components") && j["components"].is_array()) {
            for (const auto& item : j["components"]) {
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
                c.permissions = item.value("permissions", "0644");
                c.create_destination = item.value("create-destination", false);
                m.components.push_back(c);
            }
        } else if (j.contains("components") && !j["components"].is_array()) {
            return std::unexpected("'components' must be an array");
        }

        return m;
    } catch (const json::parse_error& e) {
        return std::unexpected(std::string("Syntax Error: ") + e.what());
    } catch (const std::exception& e) {
        return std::unexpected(std::string("Internal Error: ") + e.what());
    }
}

} // namespace flash
