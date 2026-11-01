#pragma once

#include <asio/io_context.hpp>
#include <relay/config/config.hpp>
#include <relay/router/router.hpp>

#include <memory>

namespace relay::net {

class Server {
public:
    Server(asio::io_context& io, ServerConfig config);

    void start();

private:
    void do_accept();

    asio::io_context& io_;
    ServerConfig config_;
    Router router_;
    asio::ip::tcp::acceptor acceptor_;
};

}  // namespace relay::net
