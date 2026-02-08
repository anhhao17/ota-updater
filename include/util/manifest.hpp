#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include <expected>

namespace flash {

struct Component {
    std::string name;
    std::string type;
    std::string filename;
    std::uint64_t size = 0;
    std::string sha256;
    std::string version;
    bool force = false;
    
    std::string install_to;   
    std::string path;         
    std::string permissions = "";
    bool create_destination = false;
};

struct Manifest {
    std::string version;
    std::string hw_compatibility;
    bool force_all = false;
    std::vector<Component> components;
    std::unordered_map<std::string, std::vector<Component>> slot_components;
};

class ManifestHandler {
public:
    static std::expected<Manifest, std::string> Parse(const std::string& jsonInput);

    static bool ShouldUpdate(const Component& comp, const Manifest& manifest, const std::string& currentVersion);
    
    static int CompareVersions(const std::string& v1, const std::string& v2);
};

} // namespace flash
