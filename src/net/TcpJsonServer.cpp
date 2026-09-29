#include "net/TcpJsonServer.hpp"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>

namespace mm {
namespace {

bool setNonBlocking(int fd) {
    const int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        return false;
    }
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
}

}  // namespace

TcpJsonServer::~TcpJsonServer() { stop(); }

bool TcpJsonServer::start(std::uint16_t port) {
    stop();
    listenFd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listenFd_ < 0) {
        return false;
    }
    int yes = 1;
    setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    if (!setNonBlocking(listenFd_)) {
        ::close(listenFd_);
        listenFd_ = -1;
        return false;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(port);
    if (::bind(listenFd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        ::close(listenFd_);
        listenFd_ = -1;
        return false;
    }
    if (::listen(listenFd_, 1) < 0) {
        ::close(listenFd_);
        listenFd_ = -1;
        return false;
    }
    port_ = port;
    running_ = true;
    return true;
}

void TcpJsonServer::stop() {
    running_ = false;
    closeClient();
    if (listenFd_ >= 0) {
        ::close(listenFd_);
        listenFd_ = -1;
    }
    {
        std::lock_guard<std::mutex> lock(inMu_);
        pendingIn_.clear();
        readBuf_.clear();
    }
    {
        std::lock_guard<std::mutex> lock(outMu_);
        pendingOut_.clear();
    }
}

void TcpJsonServer::closeClient() {
    if (clientFd_ >= 0) {
        ::close(clientFd_);
        clientFd_ = -1;
    }
    readBuf_.clear();
}

void TcpJsonServer::tryAccept() {
    if (listenFd_ < 0 || clientFd_ >= 0) {
        return;
    }
    sockaddr_in addr{};
    socklen_t len = sizeof(addr);
    const int fd = ::accept(listenFd_, reinterpret_cast<sockaddr*>(&addr), &len);
    if (fd < 0) {
        return;
    }
    setNonBlocking(fd);
    clientFd_ = fd;
    readBuf_.clear();
    sendLine(R"({"type":"hello","msg":"market_maker"})");
}

void TcpJsonServer::tryRead() {
    if (clientFd_ < 0) {
        return;
    }
    char buf[4096];
    while (true) {
        const ssize_t n = ::recv(clientFd_, buf, sizeof(buf), 0);
        if (n > 0) {
            readBuf_.append(buf, static_cast<std::size_t>(n));
            std::size_t pos = 0;
            while (true) {
                const auto nl = readBuf_.find('\n', pos);
                if (nl == std::string::npos) {
                    break;
                }
                std::string line = readBuf_.substr(pos, nl - pos);
                if (!line.empty() && line.back() == '\r') {
                    line.pop_back();
                }
                if (!line.empty()) {
                    std::lock_guard<std::mutex> lock(inMu_);
                    pendingIn_.push_back(std::move(line));
                }
                pos = nl + 1;
            }
            readBuf_.erase(0, pos);
        } else if (n == 0) {
            closeClient();
            return;
        } else {
            break;  // EAGAIN / no data
        }
    }
}

void TcpJsonServer::poll() {
    if (!running_) {
        return;
    }
    tryAccept();
    tryRead();

    std::vector<std::string> out;
    {
        std::lock_guard<std::mutex> lock(outMu_);
        out.swap(pendingOut_);
    }
    if (clientFd_ < 0) {
        return;
    }
    for (const auto& line : out) {
        std::string payload = line;
        if (payload.empty() || payload.back() != '\n') {
            payload.push_back('\n');
        }
        const char* data = payload.data();
        std::size_t left = payload.size();
        while (left > 0) {
            const ssize_t n = ::send(clientFd_, data, left, 0);
            if (n > 0) {
                data += n;
                left -= static_cast<std::size_t>(n);
            } else {
                closeClient();
                return;
            }
        }
    }
}

void TcpJsonServer::sendLine(const std::string& line) {
    std::lock_guard<std::mutex> lock(outMu_);
    pendingOut_.push_back(line);
}

std::vector<std::string> TcpJsonServer::drainIncoming() {
    std::lock_guard<std::mutex> lock(inMu_);
    std::vector<std::string> out;
    out.swap(pendingIn_);
    return out;
}

}  // namespace mm
