//
// Created by rs590 on 2/20/23.
//

#include <logger.hpp>

Logger::Logger(const std::string &filename) {
    // Extract the directory path from the given filename
    const std::string dir_path = filename.substr(0, filename.find_last_of('/'));

    // Create the directory if it does not exist
    if (!dir_path.empty()) {
        std::filesystem::create_directories(dir_path);
    }

    // Open the file in append mode and create it if it does not exist
    file_.open(filename, std::fstream::out | std::fstream::app);

    // Check if the file was successfully opened
    if (!file_.is_open()) {
        // Throw an exception if the file could not be opened
        throw std::runtime_error("Failed to open log file: " + filename);
    }
}

Logger::~Logger() {
    if (file_.is_open()) {
        file_.close();
    }
}

Logger &Logger::GetInstance(const std::string &filename) {
    static Logger instance(filename);
    return instance;
}

void Logger::log(const std::string &message) {
    // Check if the file is open before writing to it
    if (file_.is_open()) {
        std::lock_guard<std::mutex> lock(mutex_);
        file_ << message << std::endl;
    } else {
        // Throw an exception if the file is not open
        throw std::runtime_error("Log file is not open");
    }
}

std::string getClientIP(int fd) {
    sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    if (getpeername(fd, (struct sockaddr *)&addr, &addr_len) != 0) {
        // error handling
        std::cerr << "Failed to get peer name" << std::endl;
        return "";
    }
    return inet_ntoa(addr.sin_addr);
}

std::string getCurrentTime() {
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    std::tm now_tm = *std::gmtime(&now_c);
    char time_str[80];
    std::strftime(time_str, sizeof(time_str), "%a %b %d %H:%M:%S %Y", &now_tm);
    return std::string{time_str};
}

void Logger::request(const std::string &id, int fd, const std::string &request) {
    std::string client_ip = getClientIP(fd);
    std::string request_line = HTTP_Parser::get_request_line(request);
    std::string time_str = getCurrentTime();

    // Format log message and call log method
    std::string message = id + ": \"" + request_line + "\" from " + client_ip + " @ " + time_str;
    log(message);
}

void Logger::not_in_cache(const std::string &id) {
    std::string message = id + ": not in cache";
    log(message);
}

void Logger::cache_status(const std::string &id, std::shared_ptr<CacheEntry> entry) {
    std::ostringstream oss;
    oss << "in cache, ";
    if (entry->isFresh()) {
        oss << "valid";
    } else if (entry->isExpired()) {
        std::time_t t = std::chrono::system_clock::to_time_t(entry->getExpireTime());
        std::tm tm = *std::localtime(&t);
        oss << "but expired at " << std::put_time(&tm, "%c %Z");
    } else if (entry->isMustRevalidate()) {
        oss << "requires validation";
    }
    std::string message = id + ": " + oss.str();
    log(message);
}

void Logger::forward_request(const std::string &id, const std::string &request) {
    std::string request_line = HTTP_Parser::get_request_line(request);
    std::string host;
    std::tie(host, std::ignore) = HTTP_Parser::extract_host_and_port(request);
    std::string message = id + ": Requesting \"" + request_line + "\" from " + host;
    log(message);
}

void Logger::no_store(const std::string &id) {
    std::string message = id + ": not cacheable, \"no-store\" founded.";
    log(message);
}

void Logger::received_response(const std::string &id, const std::string &host, const std::string &response) {
    std::string response_line = HTTP_Parser::get_request_line(response);
    std::string message = id + ": Received \"" + response_line + "\" from " + host;
    log(message);
}

void Logger::cache_result(const std::string &id, std::shared_ptr<CacheEntry> entry) {
    std::ostringstream oss;
    oss << "cached, ";
    if (entry->isNeverExpires()) {
        oss << "never expires";
    } else if (entry->isNoCache() || entry->isMustRevalidate()) {
        oss << "but requires re-validation";
    } else {
        std::time_t t = std::chrono::system_clock::to_time_t(entry->getExpireTime());
        std::tm tm = *std::localtime(&t);
        oss << "expires at " << std::put_time(&tm, "%c %Z");
    }
    std::string message = id + ": " + oss.str();
    log(message);
}

void Logger::responding(const std::string &id, const std::string &response) {
    std::string response_line = HTTP_Parser::get_request_line(response);
    std::string message = id + ": Responding \"" + response_line + "\"";
    log(message);
}

void Logger::tunnel_closed(const std::string &id) {
    std::string message = id + ": Tunnel closed";
    log(message);
}

void Logger::note(const std::string &message) {
    log("[INFO] " + message);
}

void Logger::note_with_id(const std::string &id, const std::string &message) {
    log(id + ": [INFO] " + message);
}

void Logger::warning(const std::string &message) {
    log("[WARN] " + message);
}

void Logger::error(const std::string &message) {
    log("[ERROR] " + message);
}
