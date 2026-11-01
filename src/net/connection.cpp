#include <relay/net/connection.hpp>

#include <relay/http/request.hpp>
#include <relay/log.hpp>

namespace relay::net {

Connection::Connection(asio::ip::tcp::socket socket, Router router)
    : socket_(std::move(socket)), router_(std::move(router)) {}

void Connection::start() { read_some(); }

void Connection::read_some() {
    auto self = shared_from_this();
    socket_.async_read_some(
        asio::buffer(read_buf_),
        [this, self](std::error_code ec, std::size_t n) {
            if (ec) {
                return;
            }
            buffer_.append(read_buf_.data(), n);
            process_buffer();
        });
}

void Connection::process_buffer() {
    for (;;) {
        const auto parsed = http::parse_request(buffer_);
        if (parsed.error) {
            http::Response bad;
            bad.status = 400;
            bad.reason = "Bad Request";
            bad.headers["Content-Type"] = "text/plain; charset=utf-8";
            bad.body = *parsed.error;
            write_response(http::serialize_response(bad, false), false);
            return;
        }
        if (!parsed.complete || !parsed.request) {
            read_some();
            return;
        }

        const auto& req = *parsed.request;
        const auto route = router_.match(req);
        http::Response response = router_.handle(req, route);

        const std::string& conn = http::header(req, "connection");
        const bool keep = req.version == "HTTP/1.1" && conn != "close" && req.method == "GET";

        buffer_.erase(0, parsed.consumed);
        write_response(http::serialize_response(response, keep), keep);
        return;
    }
}

void Connection::write_response(std::string data, bool keep_alive) {
    auto self = shared_from_this();
    asio::async_write(
        socket_, asio::buffer(data),
        [this, self, keep_alive](std::error_code ec, std::size_t) {
            if (ec) {
                return;
            }
            if (!keep_alive) {
                std::error_code ignore;
                socket_.shutdown(asio::ip::tcp::socket::shutdown_both, ignore);
                socket_.close(ignore);
                return;
            }
            if (buffer_.empty()) {
                read_some();
            } else {
                process_buffer();
            }
        });
}

void Connection::on_request_ready() { process_buffer(); }

void Connection::reset() { buffer_.clear(); }

}  // namespace relay::net
