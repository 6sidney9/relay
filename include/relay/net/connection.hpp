#pragma once

#include <asio/ip/tcp.hpp>
#include <relay/router/router.hpp>

#include <array>
#include <memory>
#include <string>

namespace relay::net {

class Connection : public std::enable_shared_from_this<Connection> {
public:
    Connection(asio::ip::tcp::socket socket, Router router);

    void start();

private:
    void read_some();
    void process_buffer();
    void on_request_ready();
    void write_response(std::string data, bool keep_alive);
    void reset();

    asio::ip::tcp::socket socket_;
    Router router_;
    std::string buffer_;
    static constexpr std::size_t kReadChunk = 8192;
    std::array<char, kReadChunk> read_buf_{};
};

}  // namespace relay::net
