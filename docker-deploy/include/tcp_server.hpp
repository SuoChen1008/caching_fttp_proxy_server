//
// Created by Rocco Su on 2/16/23.
//
#ifndef TCP_SERVER_HPP
#define TCP_SERVER_HPP

#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <uuid/uuid.h>

#include <cerrno>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>

#include "cache.hpp"
#include "http_parser.hpp"
#include "logger.hpp"
#include "tcp_client.hpp"
#include "thread_pool.hpp"

class Server {
public:
    Server(std::string port, size_t numThreads, Logger &logger, Cache &cache);

    ~Server();

    void start();
    void end();

private:
    static const int BACKLOG = 10;
    std::string m_port;
    int m_listenSocket;
    ThreadPool m_threadPool;
    Logger &m_logger;
    Cache &m_cache;

    std::string receive_request(int clientSocket);

    std::string generate_uuid();

    void handle_request(int clientSocket);

    void handle_GET(int clientSocket,
                    const std::string &id,
                    const std::string &host,
                    const std::string &port,
                    const std::string &request);

    void handle_miss(int clientSocket,
                     const std::string &id,
                     const std::string &host,
                     const std::string &port,
                     const std::string &request);

    void handle_refresh(int clientSocket,
                        const std::string &id,
                        const std::string &host,
                        const std::string &port,
                        const std::string &request,
                        std::shared_ptr<CacheEntry> entry);

    void handle_revalidate(int clientSocket,
                           const std::string &id,
                           const std::string &host,
                           const std::string &port,
                           const std::string &request,
                           std::shared_ptr<CacheEntry> entry);

    void handle_CONNECT(int clientSocket,
                        const std::string &id,
                        const std::string &host,
                        const std::string &port);

    void handle_POST(int clientSocket,
                     const std::string &id,
                     const std::string &host,
                     const std::string &port,
                     const std::string &request);

    void forward_data(int client_fd, int server_fd, const std::string &id);

    std::string forward_request(int clientSocket, const std::string &host, const std::string &port, const std::string &request);

    void forward_response(int clientSocket, const std::string &response);
};

#endif  // TCP_SERVER_HPP
