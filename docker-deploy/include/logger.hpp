//
// Created by rs590 on 2/20/23.
//

#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <iostream>
#include <fstream>
#include <string>
#include <mutex>
#include <filesystem>
#include <chrono>
#include <ctime>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "cache.hpp"
#include "http_parser.hpp"

class CacheEntry;

class Logger {
public:
    static Logger &GetInstance(const std::string &filename);
    
    void request(const std::string &id, int fd, const std::string &request);
    void not_in_cache(const std::string &id);
    void cache_status(const std::string &id, std::shared_ptr<CacheEntry> entry);
    void forward_request(const std::string &id, const std::string &request);
    void received_response(const std::string &id, const std::string &host, const std::string &response);
    void cache_result(const std::string &id, std::shared_ptr<CacheEntry> entry);
    void no_store(const std::string &id);
    void responding(const std::string &id, const std::string &response);
    void tunnel_closed(const std::string &id);
    void note(const std::string &message);
    void note_with_id(const std::string &id, const std::string &message);
    void warning(const std::string &message);
    void error(const std::string &message);

private:
    Logger(const std::string &filename);
    void log(const std::string &message);

    ~Logger();

    std::ofstream file_;
    std::mutex mutex_;
};

#endif //LOGGER_HPP
