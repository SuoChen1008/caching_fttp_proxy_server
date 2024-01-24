//
// Created by Rocco Su on 2/16/23.
//

#include <chrono>
#include <iostream>
#include <thread>

#include "cache.hpp"
#include "logger.hpp"
#include "tcp_client.hpp"
#include "tcp_server.hpp"
#include "thread_pool.hpp"

const int PORT = 12345;
const int NUMBER_OF_WORKERS = 100;
const std::string LOG_PATH = "/var/log/erss/proxy.log";
//  const std::string LOG_PATH = "./log/proxy.log";

int main() {
    std::cout << "Proxy server is running on port " << PORT << std::endl;
    // Get instance of Logger
    Logger &logger = Logger::GetInstance(LOG_PATH);
    // Get instance of Cache
    Cache &cache = Cache::getInstance(logger);
    // 创建服务器实例并启动
    Server server(std::to_string(PORT),
                  NUMBER_OF_WORKERS,
                  logger,
                  cache);
    server.start();
    return 0;
}
