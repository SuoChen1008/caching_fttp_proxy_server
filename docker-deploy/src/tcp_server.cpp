//
// Created by Rocco Su on 2/16/23.
//
#include "tcp_server.hpp"

Server::Server(std::string port, size_t numThreads, Logger &logger, Cache &cache) : m_port(std::move(port)),
                                                                                    m_threadPool(numThreads),
                                                                                    m_logger(logger),
                                                                                    m_cache(cache) {
    // Step 1: 获取地址信息
    addrinfo hints{}, *serverInfo, *p;
    std::memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;      // 任何IP地址（IPv4或IPv6）
    hints.ai_socktype = SOCK_STREAM;  // 流式套接字
    hints.ai_flags = AI_PASSIVE;      // 用于监听的地址
    int status = ::getaddrinfo(nullptr, m_port.c_str(), &hints, &serverInfo);
    if (status != 0) {
        throw std::runtime_error("getaddrinfo error: " + std::string(gai_strerror(status)));
    }

    // Step 2: 创建套接字并绑定
    for (p = serverInfo; p != nullptr; p = p->ai_next) {
        m_listenSocket = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (m_listenSocket == -1) {
            continue;
        }

        int yes = 1;
        if (::setsockopt(m_listenSocket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            ::close(m_listenSocket);
            ::freeaddrinfo(serverInfo);
            throw std::runtime_error("setsockopt error: " + std::string(std::strerror(errno)));
        }

        if (::bind(m_listenSocket, p->ai_addr, p->ai_addrlen) == -1) {
            ::close(m_listenSocket);
            continue;
        }

        break;
    }

    ::freeaddrinfo(serverInfo);
    // Step 3: 监听套接字
    if (p == nullptr) {
        throw std::runtime_error("Failed to bind to any address.");
    }

    if (::listen(m_listenSocket, BACKLOG) == -1) {
        throw std::runtime_error("listen error: " + std::string(std::strerror(errno)));
    }
}

Server::~Server() {
    end();
}

void Server::end() {
    // Stop all worker threads.
    m_threadPool.~ThreadPool();
    // Close all fd.
    close(m_listenSocket);
}

void Server::start() {
    while (true) {
        int clientSocket = ::accept(m_listenSocket, nullptr, nullptr);
        if (clientSocket == -1) {
            // 将错误记录到日志中，并继续等待连接请求
            std::cerr << "accept error: " << std::strerror(errno) << std::endl;
            continue;
        }
        // 提交客户端请求到线程池处理
        m_threadPool.enqueue([this, clientSocket]() {
            try {
                handle_request(clientSocket);
            } catch (const std::exception &e) {
                // 将异常记录到日志中，并关闭客户端连接
                std::cerr << "handleClient error: " << e.what() << std::endl;
                ::close(clientSocket);
            }
        });
    }
}

std::string Server::receive_request(int clientSocket) {
    char buffer[BUFFER_SIZE];
    int numBytes = ::recv(clientSocket, buffer, sizeof(buffer), 0);
    if (numBytes == -1) {
        throw std::runtime_error("Failed to receive message from client.");
    }
    buffer[numBytes] = '\0';
    // std::cout << "Received request from client:\n" << buffer << std::endl;
    return std::string(buffer, numBytes);
}

std::string Server::generate_uuid() {
    uuid_t uuid;
    uuid_generate(uuid);
    char uuid_str[37];
    uuid_unparse(uuid, uuid_str);
    return std::string(uuid_str);
}

std::string Server::forward_request(int clientSocket, const std::string &host, const std::string &port, const std::string &request) {
    // 连接目标服务器
    Client client;
    if (!client.connect(host, port)) {
        throw std::runtime_error("Failed to connect to target server.");
    }
    // std::cout << "Connected to target server: " << host << ":" << port << std::endl;

    // 发送请求到目标服务器
    if (!client.send(request)) {
        throw std::runtime_error("Failed to send request to target server.");
    }
    // std::cout << "Sent request to target server:\n" << request << std::endl;

    // 接收目标服务器的响应
    std::string response = client.receive(clientSocket);
    // std::cout << "Received response from target server:\n" << response << std::endl;

    return response;
}

void Server::forward_response(int clientSocket, const std::string &response) {
    ssize_t total_len = response.size();
    const char *buf = response.c_str();
    ssize_t len = ::send(clientSocket, buf, total_len, 0);
    if (len <0) {
        throw std::runtime_error("Failed to send response to client.");
    }
}

void Server::handle_GET(int clientSocket, const std::string &id, const std::string &host, const std::string &port,
                        const std::string &request) {
    // 尝试从缓存中获取条目
    std::shared_ptr<CacheEntry> entry;
    bool found = m_cache.get(host, entry);
    if (found) {
        // 条目已存在于缓存中
        m_logger.cache_status(id, entry);

        if (entry->isMustRevalidate() || entry->isNoCache()) {
            // 需要重新验证
            handle_revalidate(clientSocket, id, host, port, request, entry);
        } else if (entry->isFresh()) {
            // 条目未过期，直接返回缓存的响应
            m_logger.responding(id, entry->getResponse());
            forward_response(clientSocket, entry->getResponse());
        } else if (entry->isExpired()) {
            // 条目已过期，但未标记为不可缓存
            handle_refresh(clientSocket, id, host, port, request, entry);
        }
    } else {
        // 条目不存在于缓存中，需要从目标服务器获取响应
        handle_miss(clientSocket, id, host, port, request);
    }
}

void Server::handle_revalidate(int clientSocket, const std::string &id, const std::string &host, const std::string &port,
                               const std::string &request, std::shared_ptr<CacheEntry> entry) {
    // 构造重新验证请求
    std::string revalidate_request = request;
    HTTP_Parser::make_revalidate_request(revalidate_request, entry->getResponse());
    m_logger.forward_request(id, revalidate_request);

    // 转发重新验证请求到目标服务器
    try {
        std::string response = forward_request(clientSocket, host, port, revalidate_request);
        // response为空说明为chunked response已进行转发
        if (response.empty()) {
            return;
        }

        // 接收到响应，记录日志
        m_logger.received_response(id, host, response);

        // 检查响应状态码
        int status_code = HTTP_Parser::get_status_code(response);

        if (status_code == 200) {
            // 重新验证成功，将响应放入缓存
            std::shared_ptr<CacheEntry> cached_entry = m_cache.insert(id, host, response);
            m_logger.cache_result(id, cached_entry);

            // 将响应转发给客户端
            m_logger.responding(id, response);
            forward_response(clientSocket, response);
        } else {
            // 重新验证失败，返回缓存的响应
            m_logger.responding(id, entry->getResponse());
            forward_response(clientSocket, entry->getResponse());
        }
    } catch (const std::runtime_error &ex) {
        // 发生异常，返回缓存的响应
        m_logger.responding(id, entry->getResponse());
        forward_response(clientSocket, entry->getResponse());
    }
}

void Server::handle_refresh(int clientSocket, const std::string &id, const std::string &host, const std::string &port,
                            const std::string &request, std::shared_ptr<CacheEntry> entry) {
    // 转发请求到目标服务器
    m_logger.forward_request(id, request);
    try {
        // 获取目标服务器的响应
        std::string response = forward_request(clientSocket, host, port, request);
        // response为空说明为chunked response已进行转发
        if (response.empty()) {
            return;
        }

        // 接收到响应，记录日志
        m_logger.received_response(id, host, response);

        if (!HTTP_Parser::has_no_store(response)) {
            // 将响应放入缓存
            std::shared_ptr<CacheEntry> cached_entry = m_cache.insert(id, host, response);
            m_logger.cache_result(id, cached_entry);
        }  else {
            m_logger.no_store(id);
        }

        // 将响应转发给客户端
        m_logger.responding(id, response);
        forward_response(clientSocket, response);
    } catch (const std::runtime_error &ex) {
        // 发生异常，返回缓存的响应
        m_logger.responding(id, entry->getResponse());
        forward_response(clientSocket, entry->getResponse());
    }
}

void Server::handle_miss(int clientSocket, const std::string &id, const std::string &host, const std::string &port,
                         const std::string &request) {
    m_logger.not_in_cache(id);
    // 转发请求到目标服务器
    m_logger.forward_request(id, request);
    try {
        // 获取目标服务器的响应
        std::string response = forward_request(clientSocket, host, port, request);
        // response为空说明为chunked response已进行转发
        if (response.empty()) {
            return;
        }

        // 接收到响应，记录日志
        m_logger.received_response(id, host, response);

        if (!HTTP_Parser::has_no_store(response)) {
            // 将响应放入缓存
            std::shared_ptr<CacheEntry> cached_entry = m_cache.insert(id, host, response);
            m_logger.cache_result(id, cached_entry);
        }  else {
            m_logger.no_store(id);
        }

        // 将响应转发给客户端
        m_logger.responding(id, response);
        forward_response(clientSocket, response);
    } catch (const std::runtime_error &ex) {
        // 发生异常，返回错误响应
        std::string error_response = HTTP_Parser::make_error_response(503, "Service Unavailable");
        m_logger.responding(id, error_response);
        forward_response(clientSocket, error_response);
    }
}

void Server::handle_request(int clientSocket) {
    try {
        // Generate UUID for request
        std::string request_id = generate_uuid();

        // Get request from fd
        std::string request = receive_request(clientSocket);

        // Handle null request
        if (request.empty()) {
            std::string error_response = HTTP_Parser::make_error_response(400, "Bad Request");
            m_logger.responding(request_id, error_response);
            forward_response(clientSocket, error_response);
        }

        // Extract request
        std::string host, port, method;
        std::tie(host, port) = HTTP_Parser::extract_host_and_port(request);
        method = HTTP_Parser::extract_http_method(request);

        // Log Request
        m_logger.request(request_id, clientSocket, request);
        // Handle CONNECT
        if (method == "CONNECT") {
            handle_CONNECT(clientSocket, request_id, host, port);
        }
        // Handle POST
        else if (method == "POST") {
            handle_POST(clientSocket, request_id, host, port, request);
        }
        // Handle GET
        else if (method == "GET") {
            handle_GET(clientSocket, request_id, host, port, request);
        }
        // Handle Other
        else {
            std::string error_response = HTTP_Parser::make_error_response(400, "Bad Request");
            m_logger.responding(request_id, error_response);
            forward_response(clientSocket, error_response);
        }

        ::close(clientSocket);
    } catch (const std::exception &e) {
        std::cerr << "Exception caught: " << e.what() << std::endl;
        ::close(clientSocket);
    }
}

void Server::handle_CONNECT(int clientSocket, const std::string &id, const std::string &host, const std::string &port) {
    // 将响应发送给客户端
    std::string response = "HTTP/1.1 200 Connection Established\r\n\r\n";
    forward_response(clientSocket, response);
    m_logger.responding(id, response);

    // 连接目标服务器
    Client server;
    if (!server.connect(host, port)) {
        throw std::runtime_error("Failed to connect to target server.");
    }

    // 处理连接
    int serverSocket = server.getFd();
    forward_data(clientSocket, serverSocket, id);
}

void Server::forward_data(int client_fd, int server_fd, const std::string &id) {
    fd_set readfds;
    int nfds = server_fd > client_fd ? server_fd + 1 : client_fd + 1;

    while (true) {
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);
        FD_SET(client_fd, &readfds);

        select(nfds, &readfds, nullptr, nullptr, nullptr);

        for (int fd : {client_fd, server_fd}) {
            if (FD_ISSET(fd, &readfds)) {
                char buffer[BUFFER_SIZE];
                int numBytes = ::recv(fd, buffer, sizeof(buffer), 0);
                if (numBytes <= 0) {
                    m_logger.tunnel_closed(id);
                    return;
                }
                int sent_len = 0;
                int total_len = numBytes;
                const char *buf = buffer;
                while (sent_len < total_len) {
                    int len = ::send(fd == client_fd ? server_fd : client_fd, buf + sent_len, total_len - sent_len, 0);
                    if (len == -1) {
                        m_logger.tunnel_closed(id);
                        return;
                    }
                    sent_len += len;
                }
            }
        }
    }
}

void Server::handle_POST(int clientSocket, const std::string &id, const std::string &host, const std::string &port,
                         const std::string &request) {
    // 获取请求体的长度
    int post_len = HTTP_Parser::get_content_length(request);

    if (post_len != -1) {
        // 获取响应体
        std::string response = forward_request(clientSocket, host, port, request);
        // response为空说明为chunked response已进行转发
        if (response.empty()) {
            return;
        }

        // 将响应发送给客户端
        forward_response(clientSocket, response);

        // 记录日志
        m_logger.request(id, clientSocket, request);
        m_logger.responding(id, response);
    } else {
        // 请求不合法，返回错误响应
        std::string error_response = HTTP_Parser::make_error_response(411, "Length Required");
        m_logger.responding(id, error_response);
        forward_response(clientSocket, error_response);
    }
}
