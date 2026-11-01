#include <relay/http/mime.hpp>

#include <string_view>
#include <unordered_map>

namespace relay::http {

namespace {

std::string extension_lower(std::string_view path) {
    const auto dot = path.rfind('.');
    if (dot == std::string_view::npos || dot + 1 >= path.size()) {
        return {};
    }
    std::string ext(path.substr(dot));
    for (char& c : ext) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return ext;
}

}  // namespace

std::string guess_content_type(std::string_view path) {
    static const std::unordered_map<std::string, std::string> kTypes = {
        {".html", "text/html; charset=utf-8"},
        {".htm", "text/html; charset=utf-8"},
        {".css", "text/css; charset=utf-8"},
        {".js", "application/javascript; charset=utf-8"},
        {".json", "application/json; charset=utf-8"},
        {".txt", "text/plain; charset=utf-8"},
        {".png", "image/png"},
        {".jpg", "image/jpeg"},
        {".jpeg", "image/jpeg"},
        {".gif", "image/gif"},
        {".svg", "image/svg+xml"},
        {".ico", "image/x-icon"},
        {".woff2", "font/woff2"},
        {".wasm", "application/wasm"},
    };

    const auto ext = extension_lower(path);
    const auto it = kTypes.find(ext);
    if (it != kTypes.end()) {
        return it->second;
    }
    return "application/octet-stream";
}

}  // namespace relay::http
