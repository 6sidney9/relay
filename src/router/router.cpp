#include <relay/router/router.hpp>

#include <relay/handlers/handlers.hpp>
#include <relay/http/request.hpp>

#include <algorithm>
#include <cctype>

namespace relay {

namespace {

std::string normalize_host(std::string host) {
    std::transform(host.begin(), host.end(), host.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    const auto colon = host.find(':');
    if (colon != std::string::npos) {
        host.resize(colon);
    }
    return host;
}

http::Response not_found() {
    http::Response r;
    r.status = 404;
    r.reason = "Not Found";
    r.headers["Content-Type"] = "text/plain; charset=utf-8";
    r.body = "no site for this Host header";
    return r;
}

}  // namespace

Router::Router(ServerConfig config) : config_(std::move(config)) {
    if (config_.sites.size() == 1) {
        default_site_ = &config_.sites.front();
    }
}

RouteMatch Router::match(const http::Request& req) const {
    RouteMatch match;
    std::string host = normalize_host(http::header(req, "host"));
    if (host.empty() && default_site_) {
        match.site = default_site_;
        return match;
    }

    for (const auto& site : config_.sites) {
        if (site.host == host) {
            match.site = &site;
            return match;
        }
    }

    if (default_site_) {
        match.site = default_site_;
    }
    return match;
}

http::Response Router::handle(const http::Request& req, const RouteMatch& route) const {
    if (!route.site) {
        return not_found();
    }

    if (route.site->proxy_url) {
        auto proxied = handlers::forward_proxy(req, *route.site->proxy_url);
        if (!proxied) {
            http::Response r;
            r.status = 502;
            r.reason = "Bad Gateway";
            r.headers["Content-Type"] = "text/plain; charset=utf-8";
            r.body = "proxy failed";
            return r;
        }
        return *proxied;
    }

    return handlers::serve_static(req, route.site->root);
}

}  // namespace relay
