//
// Created by Rocco Su on 2/16/23.
//

#ifndef TCP_CLIENT_HPP
#define TCP_CLIENT_HPP

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <iostream>
#include <string>
#include <sstream>
#include "http_parser.hpp"

const int BUFFER_SIZE = 1024;
class Client {
   public:
    Client();

    ~Client();

    bool connect(const std::string &host, const std::string &port);

    bool send(const std::string &data) const;

    std::string receive(int original_client_fd);

    void close();

    int getFd() const { return sockfd; }

   private:
    int sockfd;
    void handle_chunked_response(int original_client_fd, const std::string& response) const;
};

#endif  // TCP_CLIENT_HPP
