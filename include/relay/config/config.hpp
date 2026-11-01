#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace relay {

struct SiteConfig {
    std::string host;
    std::filesystem::path root;
    std::optional<std::string> proxy_url;
};

struct ServerConfig {
    std::string listen_host = "0.0.0.0";
    std::uint16_t listen_port = 8080;
    std::filesystem::path config_path;
    std::vector<SiteConfig> sites;
};

// Loads relay.toml. Throws std::runtime_error on parse errors.
ServerConfig load_config(const std::filesystem::path& path);

void validate_config(const ServerConfig& cfg);

}  // namespace relay
