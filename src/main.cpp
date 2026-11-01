#include <relay/config/config.hpp>
#include <relay/log.hpp>
#include <relay/net/server.hpp>

#include <asio/io_context.hpp>

#include <csignal>
#include <iostream>
#include <string>

namespace {

void print_usage() {
    std::cerr
        << "relay — small HTTP server (static files + reverse proxy)\n\n"
        << "Usage:\n"
        << "  relay run [--config path]\n"
        << "  relay check [--config path]\n\n"
        << "Default config file: relay.toml in the current directory.\n";
}

std::filesystem::path config_path(int argc, char** argv) {
    std::filesystem::path path = "relay.toml";
    for (int i = 2; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--config" && i + 1 < argc) {
            path = argv[++i];
        }
    }
    return path;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        print_usage();
        return 1;
    }

    const std::string cmd = argv[1];
    const auto cfg_file = config_path(argc, argv);

    try {
        if (cmd == "check") {
            const auto cfg = relay::load_config(cfg_file);
            relay::log::info("config ok: " + std::to_string(cfg.sites.size()) + " site(s), listen " +
                             cfg.listen_host + ":" + std::to_string(cfg.listen_port));
            return 0;
        }

        if (cmd != "run") {
            print_usage();
            return 1;
        }

        auto cfg = relay::load_config(cfg_file);
        asio::io_context io;

#if defined(_WIN32)
        asio::signal_set signals(io, SIGINT, SIGBREAK);
#else
        asio::signal_set signals(io, SIGINT, SIGTERM);
#endif
        signals.async_wait([&](std::error_code, int) { io.stop(); });

        relay::net::Server server(io, std::move(cfg));
        server.start();
        io.run();
        relay::log::info("stopped");
        return 0;
    } catch (const std::exception& ex) {
        relay::log::error(ex.what());
        return 1;
    }
}
