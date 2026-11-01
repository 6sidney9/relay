#pragma once

#include <filesystem>
#include <optional>
#include <relay/http/request.hpp>

namespace relay::handlers {

http::Response serve_static(const http::Request& req, const std::filesystem::path& root);

// Returns empty optional if upstream could not be reached in time.
std::optional<http::Response> forward_proxy(const http::Request& req,
                                            const std::string& upstream_base);

}  // namespace relay::handlers
