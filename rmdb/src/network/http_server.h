/* Minimal HTTP/1.1 server for RMDB REST interface.
   No external dependencies — raw sockets + string parsing.
   Runs in its own thread, shared database core, separate port. */

#pragma once

#include <atomic>
#include <functional>
#include <string>
#include <thread>

#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

using HttpHandler = std::function<std::string(const std::string &method,
    const std::string &path, const std::string &body)>;

class HttpServer {
public:
    HttpServer(int port, HttpHandler handler)
        : port_(port), handler_(std::move(handler)) {}

    void start() {
        thread_ = std::thread(&HttpServer::run, this);
    }

    void stop() {
        running_ = false;
        // Connect to ourselves to unblock accept()
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd >= 0) {
            struct sockaddr_in addr{};
            addr.sin_family = AF_INET;
            addr.sin_port   = htons(static_cast<uint16_t>(port_));
            addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            connect(fd, (struct sockaddr *)&addr, sizeof(addr));
            close(fd);
        }
        if (thread_.joinable()) thread_.join();
    }

private:
    int port_;
    std::atomic<bool> running_{true};
    std::thread thread_;
    HttpHandler handler_;

    // ---- Minimal HTTP parsing ----

    struct HttpRequest {
        std::string method;
        std::string path;
        std::string body;
        bool valid = false;
    };

    static std::string read_line(int fd) {
        std::string line;
        char c;
        while (read(fd, &c, 1) == 1) {
            if (c == '\r') continue;
            if (c == '\n') break;
            line += c;
        }
        return line;
    }

    static HttpRequest parse_request(int fd) {
        HttpRequest req;
        std::string request_line = read_line(fd);
        if (request_line.empty()) return req;

        // Parse "METHOD /path HTTP/1.1"
        size_t sp1 = request_line.find(' ');
        size_t sp2 = request_line.find(' ', sp1 + 1);
        if (sp1 == std::string::npos || sp2 == std::string::npos) return req;
        req.method = request_line.substr(0, sp1);
        req.path   = request_line.substr(sp1 + 1, sp2 - sp1 - 1);

        // Read headers
        int content_length = 0;
        while (true) {
            std::string header = read_line(fd);
            if (header.empty()) break;
            if (header.rfind("Content-Length:", 0) == 0 ||
                header.rfind("content-length:", 0) == 0) {
                size_t col = header.find(':');
                std::string val = header.substr(col + 1);
                // trim leading space
                size_t start = val.find_first_not_of(" \t");
                if (start != std::string::npos)
                    content_length = std::stoi(val.substr(start));
            }
        }

        // Read body
        if (content_length > 0 && content_length < 1048576) {  // 1MB max
            req.body.resize(content_length);
            size_t total = 0;
            while (total < static_cast<size_t>(content_length)) {
                ssize_t n = read(fd, req.body.data() + total,
                                 static_cast<size_t>(content_length) - total);
                if (n <= 0) break;
                total += static_cast<size_t>(n);
            }
        }
        req.valid = true;
        return req;
    }

    static void send_response(int fd, int status, const std::string &content_type,
                              const std::string &body) {
        std::string status_text = (status == 200) ? "OK" :
                                  (status == 400) ? "Bad Request" : "Internal Server Error";
        std::string response;
        response += "HTTP/1.1 " + std::to_string(status) + " " + status_text + "\r\n";
        response += "Content-Type: " + content_type + "\r\n";
        response += "Content-Length: " + std::to_string(body.size()) + "\r\n";
        response += "Connection: close\r\n";
        response += "\r\n";
        response += body;
        send(fd, response.data(), response.size(), MSG_NOSIGNAL);
    }

    void run() {
        int server_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd < 0) return;

        int opt = 1;
        setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        struct sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port   = htons(static_cast<uint16_t>(port_));
        addr.sin_addr.s_addr = INADDR_ANY;
        if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
            close(server_fd);
            std::cerr << "[http] bind failed on port " << port_ << std::endl;
            return;
        }
        listen(server_fd, 16);

        struct linger lo{1, 0};
        setsockopt(server_fd, SOL_SOCKET, SO_LINGER, &lo, sizeof(lo));

        std::cout << "[http] REST API on http://0.0.0.0:" << port_ << "/query\n";

        while (running_) {
            int client_fd = accept(server_fd, nullptr, nullptr);
            if (client_fd < 0) { if (running_) continue; else break; }

            HttpRequest req = parse_request(client_fd);
            if (!req.valid || req.method != "POST" || req.path.find("/query") != 0) {
                send_response(client_fd, 400, "application/json",
                              "{\"error\":\"bad request, use POST /query\"}");
                close(client_fd);
                continue;
            }

            std::string result = handler_(req.method, req.path, req.body);
            int status = (result.find("\"error\"") != std::string::npos) ? 400 : 200;
            send_response(client_fd, status, "application/json", result);
            close(client_fd);
        }
        close(server_fd);
    }
};
