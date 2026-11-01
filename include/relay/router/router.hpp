#pragma once

#include <relay/config/config.hpp>
#include <relay/http/request.hpp>

#include <memory>
#include <optional>

namespace relay {

struct RouteMatch {
    const SiteConfig* site = nullptr;
};

class Router {
public:
    explicit Router(ServerConfig config);

    RouteMatch match(const http::Request& req) const;

    http::Response handle(const http::Request& req, const RouteMatch& route) const;

private:
    ServerConfig config_;
    const SiteConfig* default_site_ = nullptr;
};

}  // namespace relay
