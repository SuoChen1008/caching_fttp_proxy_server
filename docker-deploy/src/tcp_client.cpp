//
// Created by Rocco Su on 2/16/23.
//

#include <tcp_client.hpp>

Client::Client() : sockfd(-1) {}

bool Client::connect(const std::string &host, const std::string &port) {
    struct addrinfo hints{
    }, *res, *p;
    std::memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int status = getaddrinfo(host.c_str(), port.c_str(), &hints, &res);
    if (status != 0) {
        std::cerr << "getaddrinfo error: " << gai_strerror(status) << std::endl;
        return false;
    }

    for (p = res; p != nullptr; p = p->ai_next) {
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd == -1) {
            std::perror("socket");
            continue;
        }
        if (::connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            std::perror("connect");
            close();
            sockfd = -1;
            continue;
        }
        break;
    }

    freeaddrinfo(res);

    if (p == nullptr) {
        std::cerr << "Failed to connect to " << host << ":" << port << std::endl;
        return false;
    }

    // 开启SO_KEEPALIVE选项检测连接断开
    int optval = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval)) == -1) {
        std::perror("setsockopt SO_KEEPALIVE");
    }

    return true;
}

bool Client::send(const std::string &data) const {
    int total_len = data.size();
    const char *buf = data.c_str();
    int sent_len = 0;
    while (sent_len < total_len) {
        int len = ::send(sockfd, buf + sent_len, total_len - sent_len, 0);
        if (len == -1) {
            std::perror("send");
            return false;
        }
        sent_len += len;
    }
    return true;
}

std::string Client::receive(int original_client_fd) {
    char buffer[BUFFER_SIZE];
    std::string response;
    int content_length = 0;
    while (true) {
        int len = ::recv(sockfd, buffer, BUFFER_SIZE, 0);
        if (len == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // 超时，认为响应接收完毕
                break;
            }
            std::perror("recv");
            break;
        } else if (len == 0) {
            // 连接关闭，认为响应接收完毕
            break;
        } else {
            response += std::string(buffer, len);
            auto headers = HTTP_Parser::parse_headers(response);
            std::string temp_content_length = HTTP_Parser::get_header_value("Content-Length", headers);

            if (content_length ==0 && !temp_content_length.empty()) {
                content_length = std::stoi(temp_content_length);
            }
            if (content_length != 0 && static_cast<int>(response.length()) >= content_length) {
                break;
            }
            if( content_length == 0) {
                break;
            }
        }
    }

    auto headers = HTTP_Parser::parse_headers(response);
    std::string chunked = HTTP_Parser::get_header_value("Transfer-Encoding", headers);
    if (!chunked.empty()) {
        handle_chunked_response(original_client_fd, response);
        return "";
    }
    return response;
}

void Client::handle_chunked_response(int original_client_fd, const std::string& response) const {
    // 将 response 转换为 const char*
    const char* response_ptr = response.c_str();

    // 设置套接字的缓冲区大小
    setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &BUFFER_SIZE, sizeof(int));
    setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, &BUFFER_SIZE, sizeof(int));

    // 一次性发送整个响应
    ssize_t sent = ::send(original_client_fd, response_ptr, response.size(), 0);
    if (static_cast<size_t>(sent) != response.size()) {
        throw std::runtime_error("Failed to forward response to client.");
    }

    // 使用缓冲区一次性接收和发送多个数据块
    char buffer[BUFFER_SIZE];
    while (true) {
        ssize_t len = ::recv(sockfd, buffer, BUFFER_SIZE, 0);
        if (len <= 0) {
            break;
        }
        sent = ::send(original_client_fd, buffer, len, 0);
        if (sent != len) {
            // 发生错误，抛出异常
            throw std::runtime_error("Failed to forward response to client.");
        }
    }
}

void Client::close() {
    if (sockfd != -1) {
        ::close(sockfd);
        sockfd = -1;
    }
}

Client::~Client() {
    close();
}
