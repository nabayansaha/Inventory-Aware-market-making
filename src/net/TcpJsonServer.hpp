#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace mm {

// Non-blocking localhost TCP server. One client. Line-delimited UTF-8 JSON.
class TcpJsonServer {
public:
    TcpJsonServer() = default;
    ~TcpJsonServer();

    TcpJsonServer(const TcpJsonServer&) = delete;
    TcpJsonServer& operator=(const TcpJsonServer&) = delete;

    bool start(std::uint16_t port);
    void stop();
    bool running() const { return running_.load(); }
    bool clientConnected() const { return clientFd_ >= 0; }
    std::uint16_t port() const { return port_; }

    // Accept pending connection / drain socket. Call from UI/sim thread.
    void poll();

    // Outgoing lines to the connected client (appends '\n').
    void sendLine(const std::string& line);

    // Pop all complete incoming lines since last poll.
    std::vector<std::string> drainIncoming();

private:
    void closeClient();
    void tryAccept();
    void tryRead();

    int listenFd_ = -1;
    int clientFd_ = -1;
    std::uint16_t port_ = 0;
    std::atomic<bool> running_{false};
    std::string readBuf_;
    std::mutex outMu_;
    std::vector<std::string> pendingOut_;
    std::mutex inMu_;
    std::vector<std::string> pendingIn_;
};

}  // namespace mm
