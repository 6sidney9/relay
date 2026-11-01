#include <relay/handlers/handlers.hpp>
#include <relay/http/mime.hpp>

#include <fstream>
#include <sstream>

namespace relay::handlers {

namespace {

http::Response make_error(int code, std::string_view reason, std::string_view body) {
    http::Response r;
    r.status = code;
    r.reason = std::string(reason);
    r.headers["Content-Type"] = "text/plain; charset=utf-8";
    r.body.assign(body.begin(), body.end());
    return r;
}

std::optional<std::filesystem::path> resolve_path(const std::filesystem::path& root,
                                                  std::string_view target) {
    if (target.empty() || target[0] != '/') {
        return std::nullopt;
    }

    std::string path_part(target);
    const auto q = path_part.find('?');
    if (q != std::string::npos) {
        path_part.resize(q);
    }

    if (path_part.find("..") != std::string::npos) {
        return std::nullopt;
    }

    std::filesystem::path rel = path_part.substr(1);
    if (rel.empty()) {
        rel = "index.html";
    }

    const auto root_abs = std::filesystem::absolute(root).lexically_normal();
    auto candidate = (root_abs / rel).lexically_normal();

    for (const auto& part : candidate) {
        if (part == "..") {
            return std::nullopt;
        }
    }

    const auto root_str = root_abs.generic_string();
    const auto cand_str = candidate.generic_string();
    if (cand_str.size() < root_str.size() ||
        cand_str.compare(0, root_str.size(), root_str) != 0) {
        return std::nullopt;
    }
    if (cand_str.size() > root_str.size() && cand_str[root_str.size()] != '/') {
        return std::nullopt;
    }

    if (std::filesystem::is_directory(candidate)) {
        return candidate / "index.html";
    }
    return candidate;
}

std::string read_file(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("read failed");
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

}  // namespace

http::Response serve_static(const http::Request& req, const std::filesystem::path& root) {
    if (req.method != "GET" && req.method != "HEAD") {
        return make_error(405, "Method Not Allowed", "method not allowed");
    }

    const auto file = resolve_path(root, req.target);
    if (!file) {
        return make_error(400, "Bad Request", "bad path");
    }

    if (!std::filesystem::exists(*file) || !std::filesystem::is_regular_file(*file)) {
        return make_error(404, "Not Found", "not found");
    }

    http::Response ok;
    ok.status = 200;
    ok.reason = "OK";
    ok.headers["Content-Type"] = http::guess_content_type(file->generic_string());
    if (req.method == "GET") {
        try {
            ok.body = read_file(*file);
        } catch (...) {
            return make_error(500, "Internal Server Error", "read error");
        }
    }
    return ok;
}

}  // namespace relay::handlers
