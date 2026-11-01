#include <relay/config/config.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace relay {

namespace {

std::string trim(std::string_view s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) {
        s.remove_prefix(1);
    }
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) {
        s.remove_suffix(1);
    }
    return std::string(s);
}

std::string strip_quotes(std::string s) {
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"') {
        return s.substr(1, s.size() - 2);
    }
    return s;
}

std::pair<std::string, std::uint16_t> parse_listen(const std::string& value) {
    const auto colon = value.rfind(':');
    if (colon == std::string::npos || colon == 0) {
        throw std::runtime_error("listen must look like 0.0.0.0:8080");
    }
    const std::string host = value.substr(0, colon);
    const int port = std::stoi(value.substr(colon + 1));
    if (port < 1 || port > 65535) {
        throw std::runtime_error("listen port out of range");
    }
    return {host, static_cast<std::uint16_t>(port)};
}

std::string table_host_key(const std::string& table_line) {
    const auto lb = table_line.find('[');
    const auto rb = table_line.find(']');
    if (lb == std::string::npos || rb == std::string::npos || rb <= lb + 2) {
        return {};
    }
    return table_line.substr(lb + 2, rb - lb - 2);
}

}  // namespace

ServerConfig load_config(const std::filesystem::path& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("cannot open config: " + path.string());
    }

    ServerConfig cfg;
    cfg.config_path = std::filesystem::absolute(path).parent_path();

    SiteConfig* current = nullptr;
    std::string line;
    int line_no = 0;

    while (std::getline(in, line)) {
        ++line_no;
        const std::string trimmed = trim(line);
        if (trimmed.empty() || trimmed.front() == '#') {
            continue;
        }

        if (trimmed.front() == '[' && trimmed.back() == ']') {
            if (trimmed.rfind("[[site.", 0) == 0) {
                cfg.sites.emplace_back();
                current = &cfg.sites.back();
                current->host = table_host_key(trimmed);
                if (current->host.empty()) {
                    throw std::runtime_error("line " + std::to_string(line_no) +
                                             ": bad [[site.name]] header");
                }
                std::transform(current->host.begin(), current->host.end(), current->host.begin(),
                               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            } else {
                throw std::runtime_error("line " + std::to_string(line_no) + ": unknown section");
            }
            continue;
        }

        const auto eq = trimmed.find('=');
        if (eq == std::string::npos) {
            throw std::runtime_error("line " + std::to_string(line_no) + ": expected key = value");
        }

        const std::string key = trim(trimmed.substr(0, eq));
        const std::string value = strip_quotes(trim(trimmed.substr(eq + 1)));

        if (key == "listen") {
            auto [host, port] = parse_listen(value);
            cfg.listen_host = host;
            cfg.listen_port = port;
            continue;
        }

        if (!current) {
            throw std::runtime_error("line " + std::to_string(line_no) +
                                     ": key/value outside [[site.*]] block");
        }

        if (key == "root") {
            current->root = value;
            if (!current->root.is_absolute()) {
                current->root = cfg.config_path / current->root;
            }
        } else if (key == "proxy") {
            current->proxy_url = value;
        } else {
            throw std::runtime_error("line " + std::to_string(line_no) + ": unknown key " + key);
        }
    }

    validate_config(cfg);
    return cfg;
}

void validate_config(const ServerConfig& cfg) {
    if (cfg.sites.empty()) {
        throw std::runtime_error("config needs at least one [[site.host]] block");
    }

    for (const auto& site : cfg.sites) {
        if (site.host.empty()) {
            throw std::runtime_error("site block missing host name");
        }
        const bool has_root = !site.root.empty();
        const bool has_proxy = site.proxy_url.has_value();
        if (has_root == has_proxy) {
            throw std::runtime_error("site '" + site.host +
                                     "' needs exactly one of root= or proxy=");
        }
        if (has_root && !std::filesystem::exists(site.root)) {
            throw std::runtime_error("root path does not exist for site '" + site.host + "': " +
                                     site.root.string());
        }
    }
}

}  // namespace relay
