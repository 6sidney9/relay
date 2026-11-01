#include <relay/handlers/handlers.hpp>

#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/read.hpp>
#include <asio/write.hpp>

#include <cctype>
#include <sstream>

namespace relay::handlers {

namespace {

struct Upstream {
    std::string scheme;
    std::string host;
    std::uint16_t port = 80;
    std::string base_path;
};

std::optional<Upstream> parse_upstream(const std::string& url) {
    Upstream u;
    std::string_view v(url);
    const auto scheme_end = v.find("://");
    if (scheme_end == std::string_view::npos) {
        return std::nullopt;
    }
    u.scheme = std::string(v.substr(0, scheme_end));
    v.remove_prefix(scheme_end + 3);

    const auto path_start = v.find('/');
    const std::string_view host_port = path_start == std::string_view::npos ? v : v.substr(0, path_start);
    if (path_start != std::string_view::npos) {
        u.base_path = std::string(v.substr(path_start));
    }

    const auto colon = host_port.rfind(':');
    if (colon != std::string_view::npos) {
        u.host = std::string(host_port.substr(0, colon));
        try {
            u.port = static_cast<std::uint16_t>(std::stoi(std::string(host_port.substr(colon + 1))));
        } catch (...) {
            return std::nullopt;
        }
    } else {
        u.host = std::string(host_port);
        u.port = (u.scheme == "https") ? 443 : 80;
    }

    if (u.scheme != "http") {
        return std::nullopt;
    }
    return u;
}

std::string build_forwarded_request(const http::Request& req, const Upstream& up,
                                    std::string_view path_and_query) {
    std::ostringstream out;
    out << req.method << ' ' << path_and_query << " HTTP/1.1\r\n";

    for (const auto& [name, value] : req.headers) {
        if (name == "host" || name == "connection" || name == "proxy-connection" ||
            name == "keep-alive" || name == "transfer-encoding") {
            continue;
        }
        out << name << ": " << value << "\r\n";
    }

    out << "Host: " << up.host;
    if ((up.scheme == "http" && up.port != 80) || (up.scheme == "https" && up.port != 443)) {
        out << ':' << up.port;
    }
    out << "\r\nConnection: close\r\n";

    if (!req.body.empty()) {
        out << "Content-Length: " << req.body.size() << "\r\n";
    }
    out << "\r\n";
    out << req.body;
    return out.str();
}

std::string merge_path(const Upstream& up, std::string_view target) {
    std::string path(target);
    const auto q = path.find('?');
    std::string query;
    if (q != std::string::npos) {
        query = path.substr(q);
        path.resize(q);
    }

    std::string merged = up.base_path;
    if (!merged.empty() && merged.back() == '/' && !path.empty() && path.front() == '/') {
        path.erase(0, 1);
    }
    if (path.empty() || path == "/") {
        if (merged.empty()) {
            merged = "/";
        }
    } else {
        merged += path;
    }
    merged += query;
    return merged;
}

http::Response make_gateway_error(std::string_view msg) {
    http::Response r;
    r.status = 502;
    r.reason = "Bad Gateway";
    r.headers["Content-Type"] = "text/plain; charset=utf-8";
    r.body.assign(msg.begin(), msg.end());
    return r;
}

}  // namespace

std::optional<http::Response> forward_proxy(const http::Request& req,
                                            const std::string& upstream_base) {
    const auto up = parse_upstream(upstream_base);
    if (!up) {
        return make_gateway_error("bad upstream url");
    }

    asio::io_context io;
    asio::ip::tcp::socket socket(io);
    asio::ip::tcp::resolver resolver(io);

    try {
        const auto endpoints = resolver.resolve(up->host, std::to_string(up->port));
        asio::connect(socket, endpoints);
    } catch (...) {
        return make_gateway_error("upstream connect failed");
    }

    const auto payload = build_forwarded_request(req, *up, merge_path(*up, req.target));
    try {
        asio::write(socket, asio::buffer(payload));
    } catch (...) {
        return make_gateway_error("upstream write failed");
    }

    asio::streambuf response_buf;
    asio::error_code ec;
    asio::read(socket, response_buf, asio::transfer_at_least(1), ec);
    if (ec && ec != asio::error::eof) {
        return make_gateway_error("upstream read failed");
    }

    const std::string raw(asio::buffers_begin(response_buf.data()),
                          asio::buffers_end(response_buf.data()));

    const auto header_end = raw.find("\r\n\r\n");
    if (header_end == std::string::npos) {
        return make_gateway_error("bad upstream response");
    }

    const std::string_view header_part(raw.data(), header_end);
    const auto status_line_end = header_part.find("\r\n");
    if (status_line_end == std::string_view::npos) {
        return make_gateway_error("bad upstream status line");
    }

    const std::string_view status_line = header_part.substr(0, status_line_end);
    const auto sp1 = status_line.find(' ');
    const auto sp2 = status_line.find(' ', sp1 + 1);
    if (sp1 == std::string_view::npos || sp2 == std::string_view::npos) {
        return make_gateway_error("bad upstream status line");
    }

    http::Response out;
    out.status = std::stoi(std::string(status_line.substr(sp1 + 1, sp2 - sp1 - 1)));
    out.reason = std::string(status_line.substr(sp2 + 1));

    std::size_t pos = status_line_end + 2;
    while (pos < header_part.size()) {
        const auto eol = header_part.find("\r\n", pos);
        if (eol == std::string_view::npos) {
            break;
        }
        const auto line = header_part.substr(pos, eol - pos);
        pos = eol + 2;
        if (line.empty()) {
            continue;
        }
        const auto colon = line.find(':');
        if (colon == std::string_view::npos) {
            continue;
        }
        std::string name(line.substr(0, colon));
        std::string value(line.substr(colon + 1));
        while (!value.empty() && value.front() == ' ') {
            value.erase(0, 1);
        }
        if (name == "Transfer-Encoding" || name == "Connection") {
            continue;
        }
        out.headers[name] = value;
    }

    out.body = raw.substr(header_end + 4);
    return out;
}

}  // namespace relay::handlers
