#include <relay/http/request.hpp>

#include <algorithm>
#include <cctype>
#include <sstream>

namespace relay::http {

namespace {

std::string to_lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

bool iequals(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

std::optional<std::size_t> find_header_end(std::string_view buf) {
    const auto pos = buf.find("\r\n\r\n");
    if (pos == std::string_view::npos) {
        return std::nullopt;
    }
    return pos + 4;
}

std::string trim(std::string_view s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) {
        s.remove_prefix(1);
    }
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) {
        s.remove_suffix(1);
    }
    return std::string(s);
}

bool parse_request_line(std::string_view line, Request& out, std::string& err) {
    const auto sp1 = line.find(' ');
    if (sp1 == std::string_view::npos) {
        err = "bad request line";
        return false;
    }
    const auto sp2 = line.find(' ', sp1 + 1);
    if (sp2 == std::string_view::npos) {
        err = "bad request line";
        return false;
    }
    out.method = std::string(line.substr(0, sp1));
    out.target = std::string(line.substr(sp1 + 1, sp2 - sp1 - 1));
    out.version = std::string(line.substr(sp2 + 1));
    if (out.version.rfind("HTTP/1.", 0) != 0) {
        err = "unsupported HTTP version";
        return false;
    }
    return true;
}

bool parse_headers(std::string_view header_block, Request& out, std::string& err) {
    std::size_t pos = 0;
    while (pos < header_block.size()) {
        const auto eol = header_block.find("\r\n", pos);
        if (eol == std::string_view::npos) {
            break;
        }
        const auto line = header_block.substr(pos, eol - pos);
        pos = eol + 2;
        if (line.empty()) {
            continue;
        }
        const auto colon = line.find(':');
        if (colon == std::string_view::npos) {
            err = "malformed header";
            return false;
        }
        auto name = to_lower(trim(line.substr(0, colon)));
        auto value = trim(line.substr(colon + 1));
        out.headers[name] = std::move(value);
    }
    return true;
}

}  // namespace

ParseResult parse_request(std::string_view buffer, std::size_t max_header_bytes,
                          std::size_t max_body_bytes) {
    ParseResult result;

    const auto header_end = find_header_end(buffer);
    if (!header_end) {
        if (buffer.size() > max_header_bytes) {
            result.error = "header too large";
        }
        return result;
    }

    if (*header_end > max_header_bytes) {
        result.error = "header too large";
        return result;
    }

    const std::string_view header_part = buffer.substr(0, *header_end);
    const auto first_line_end = header_part.find("\r\n");
    if (first_line_end == std::string_view::npos) {
        result.error = "malformed request";
        return result;
    }

    Request req;
    std::string err;
    if (!parse_request_line(header_part.substr(0, first_line_end), req, err)) {
        result.error = err;
        return result;
    }

    if (!parse_headers(header_part.substr(first_line_end + 2), req, err)) {
        result.error = err;
        return result;
    }

    std::size_t body_len = 0;
    const auto cl = req.headers.find("content-length");
    if (cl != req.headers.end()) {
        try {
            body_len = static_cast<std::size_t>(std::stoull(cl->second));
        } catch (...) {
            result.error = "bad Content-Length";
            return result;
        }
        if (body_len > max_body_bytes) {
            result.error = "body too large";
            return result;
        }
    }

    const std::size_t total = *header_end + body_len;
    if (buffer.size() < total) {
        return result;
    }

    if (body_len > 0) {
        req.body.assign(buffer.substr(*header_end, body_len));
    }

    result.complete = true;
    result.consumed = total;
    result.request = std::move(req);
    return result;
}

std::string serialize_response(const Response& response, bool keep_alive) {
    std::ostringstream out;
    out << "HTTP/1.1 " << response.status << ' ' << response.reason << "\r\n";

    bool has_length = false;
    bool has_connection = false;
    for (const auto& [k, v] : response.headers) {
        if (iequals(k, "content-length")) {
            has_length = true;
        }
        if (iequals(k, "connection")) {
            has_connection = true;
        }
        out << k << ": " << v << "\r\n";
    }
    if (!has_length) {
        out << "Content-Length: " << response.body.size() << "\r\n";
    }
    if (!has_connection) {
        out << "Connection: " << (keep_alive ? "keep-alive" : "close") << "\r\n";
    }
    out << "\r\n";
    out << response.body;
    return out.str();
}

const std::string& header(const Request& req, std::string_view name) {
    static const std::string kEmpty;
    const auto key = to_lower(std::string(name));
    const auto it = req.headers.find(key);
    if (it == req.headers.end()) {
        return kEmpty;
    }
    return it->second;
}

}  // namespace relay::http
