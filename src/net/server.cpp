#include <relay/net/server.hpp>

#include <relay/log.hpp>
#include <relay/net/connection.hpp>

namespace relay::net {

Server::Server(asio::io_context& io, ServerConfig config)
    : io_(io),
      config_(std::move(config)),
      router_(config_),
      acceptor_(io_, asio::ip::tcp::endpoint(asio::ip::make_address(config_.listen_host),
                                             config_.listen_port)) {
    acceptor_.set_option(asio::ip::tcp::acceptor::reuse_address(true));
}

void Server::start() {
    log::info("listening on " + config_.listen_host + ":" + std::to_string(config_.listen_port));
    do_accept();
}

void Server::do_accept() {
    acceptor_.async_accept([this](std::error_code ec, asio::ip::tcp::socket socket) {
        if (!ec) {
            std::make_shared<Connection>(std::move(socket), router_)->start();
        } else {
            log::warn("accept failed: " + ec.message());
        }
        do_accept();
    });
}

}  // namespace relay::net
