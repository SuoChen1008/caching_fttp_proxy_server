#include "http_parser.hpp"

std::map<std::string, std::string> HTTP_Parser::parse_headers(const std::string &response) {
    std::map<std::string, std::string> headers;

    std::istringstream iss(response);
    std::string line;
    while (std::getline(iss, line) && line != "\r") {
        auto colon_pos = line.find(": ");
        if (colon_pos != std::string::npos) {
            auto key = line.substr(0, colon_pos);
            auto value = line.substr(colon_pos + 2);
            headers[key] = value;
        }
    }

    return headers;
}

std::string HTTP_Parser::get_header_value(const std::string &header,
                                          const std::map<std::string, std::string> &headers) {
    auto it = headers.find(header);
    if (it != headers.end()) {
        return it->second;
    } else {
        return "";
    }
}

std::unordered_map<std::string, std::string> HTTP_Parser::parse_cache_control(const std::string &cache_control) {
    std::unordered_map<std::string, std::string> result;

    // 去除Cache-Control字符串中的换行符
    std::string cache_control_clean = cache_control;
    cache_control_clean.erase(std::remove(cache_control_clean.begin(), cache_control_clean.end(), '\r'), cache_control_clean.end());
    cache_control_clean.erase(std::remove(cache_control_clean.begin(), cache_control_clean.end(), '\n'), cache_control_clean.end());

    // 以逗号为分隔符将Cache-Control字符串分割成多个子字符串
    std::istringstream ss(cache_control_clean);
    std::string token;
    while (std::getline(ss, token, ',')) {
        // 去除子字符串开头和结尾的空格
        token.erase(0, token.find_first_not_of(' '));
        token.erase(token.find_last_not_of(' ') + 1);

        // 如果子字符串中包含等号，则将其分割成键值对，并添加到结果中
        std::size_t equal_pos = token.find('=');
        if (equal_pos != std::string::npos) {
            std::string key = token.substr(0, equal_pos);
            std::string value = token.substr(equal_pos + 1);
            result[key] = value;
        }
        // 如果子字符串中不包含等号，则将其作为键，值为空字符串添加到结果中
        else {
            result[token] = "";
        }
    }

    return result;
}

bool HTTP_Parser::has_no_store(const std::string &response) {
    auto headers = HTTP_Parser::parse_headers(response);
    auto cache_control = parse_cache_control(headers["Cache-Control"]);
    return cache_control.count("no-store") > 0;
}

std::string HTTP_Parser::get_request_body(const std::string &request) {
    size_t pos = request.find("\r\n\r\n");
    if (pos != std::string::npos) {
        return request.substr(pos + 4);
    }
    return "";
}

void HTTP_Parser::make_revalidate_request(std::string &response, const std::string &old_response) {
    // 解析旧响应头
    auto headers = parse_headers(old_response);

    // 获取当前响应的ETag值
    auto etag = get_header_value("ETag", headers);

    // 如果当前响应没有ETag值，不能使用revalidate，返回
    if (etag.empty()) {
        return;
    }

    // 添加If-None-Match字段，值为当前响应的ETag值
    response += "If-None-Match: " + etag + "\r\n";

    // 获取当前响应的Last-Modified值
    auto last_modified = get_header_value("Last-Modified", headers);

    // 如果当前响应没有Last-Modified值，不能使用重新验证，返回
    if (last_modified.empty()) {
        return;
    }

    // 添加If-Modified-Since字段，值为当前响应的Last-Modified值
    response += "If-Modified-Since: " + last_modified + "\r\n";
}

int HTTP_Parser::get_status_code(const std::string &response) {
    size_t start = response.find("HTTP/1.");
    if (start != std::string::npos) {
        start += 9;
        size_t end = response.find(" ", start);
        if (end != std::string::npos) {
            std::string code_str = response.substr(start, end - start);
            try {
                int code = std::stoi(code_str);
                return code;
            } catch (std::invalid_argument &) {
                // 如果无法转换为整数，返回 -1 表示错误
                return -1;
            }
        }
    }
    // 没有找到状态码，返回 -1 表示错误
    return -1;
}

std::pair<std::string, std::string> HTTP_Parser::extract_host_and_port(const std::string &request) {
    static const std::regex re_host(R"(host:\s*([a-zA-Z0-9\.\-\[\]:]+))", std::regex_constants::icase);
    static const std::regex re_port(":\\s*([0-9]+)", std::regex_constants::icase);
    std::smatch match;

    std::string lower_request = request;
    std::transform(lower_request.begin(), lower_request.end(), lower_request.begin(), [](char c) { return std::tolower(c); });
    if (!std::regex_search(lower_request, match, re_host)) {
        // 如果没有Host头，则返回默认值
        throw std::runtime_error("Failed to extract host and port from request: no Host header found.");
    }

    std::string host = match[1].str();

    // 提取端口号
    if (std::regex_search(host, match, re_port)) {
        std::string port = match[1].str();
        if (port.empty() || !std::all_of(port.begin(), port.end(), [](char c) { return std::isdigit(c); })) {
            throw std::runtime_error("Failed to extract host and port from request: invalid port number.");
        }
        host = match.prefix().str();
        return std::make_pair(host, port);
    }

    // 没有端口号，返回默认端口
    return std::make_pair(host, "80");
}

std::string HTTP_Parser::extract_http_method(const std::string &request) {
    size_t pos = request.find(' ');
    if (pos == std::string::npos) {
        throw std::runtime_error("Invalid HTTP request: missing HTTP method.");
    }
    return request.substr(0, pos);
}

std::string HTTP_Parser::get_request_line(const std::string &request) {
    size_t end = request.find('\n');
    if (end == std::string::npos) {
        throw std::runtime_error("Invalid request format");
        return "";
    }
    return request.substr(0, end - 1);
}

std::string HTTP_Parser::make_error_response(int status_code, const std::string &status_text) {
    std::stringstream ss;
    ss << "HTTP/1.1 " << status_code << " " << status_text << "\r\n"
       << "Content-Type: text/plain\r\n"
       << "Content-Length: " << status_text.length() << "\r\n"
       << "Connection: close\r\n"
       << "\r\n"
       << status_text;
    return ss.str();
}

int HTTP_Parser::get_content_length(const std::string& request) {
    // 获取请求头部和消息体之间的空行的位置
    size_t header_end_pos = request.find("\r\n\r\n");
    if (header_end_pos == std::string::npos) {
        return -1;
    }

    // 获取 Content-Length 字段的值
    std::string content_length_str;
    size_t content_length_pos = request.find("Content-Length: ");
    if (content_length_pos != std::string::npos) {
        content_length_pos += strlen("Content-Length: ");
        size_t end_pos = request.find("\r\n", content_length_pos);
        content_length_str = request.substr(content_length_pos, end_pos - content_length_pos);
    } else {
        return -1;
    }

    // 转换 Content-Length 字段的值为整数
    int content_length = -1;
    try {
        content_length = std::stoi(content_length_str);
    } catch (const std::invalid_argument&) {
        return -1;
    } catch (const std::out_of_range&) {
        return -1;
    }

    return content_length;
}

