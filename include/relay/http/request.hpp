#pragma once

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace relay::http {

struct Request {
    std::string method;
    std::string target;
    std::string version;
    std::map<std::string, std::string, std::less<>> headers;
    std::string body;
};

struct Response {
    int status = 200;
    std::string reason = "OK";
    std::map<std::string, std::string, std::less<>> headers;
    std::string body;
};

struct ParseResult {
    bool complete = false;
    std::size_t consumed = 0;
    std::optional<Request> request;
    std::optional<std::string> error;
};

// Incremental HTTP/1.x request parser.
ParseResult parse_request(std::string_view buffer, std::size_t max_header_bytes = 65536,
                          std::size_t max_body_bytes = 16 * 1024 * 1024);

std::string serialize_response(const Response& response, bool keep_alive);

const std::string& header(const Request& req, std::string_view name);

}  // namespace relay::http
