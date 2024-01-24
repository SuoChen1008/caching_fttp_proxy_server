#ifndef HTTP_PARSER_HPP
#define HTTP_PARSER_HPP

#include <string>
#include <map>
#include <sstream>
#include <regex>
#include <algorithm>
#include <unordered_map>

class HTTP_Parser {
public:
    static std::map<std::string, std::string> parse_headers(const std::string& response);

    static std::string get_header_value(const std::string& header,
                                       const std::map<std::string, std::string>& headers);

    static std::unordered_map<std::string, std::string> parse_cache_control(const std::string& cache_control);

    static bool has_no_store(const std::string& response);

    static void make_revalidate_request(std::string& response, const std::string& old_response);

    static std::string get_request_body(const std::string& request);

    static int get_status_code(const std::string& response);

    static std::pair<std::string, std::string> extract_host_and_port(const std::string &request);

    static std::string extract_http_method(const std::string &request);

    static std::string get_request_line(const std::string &request);

    static std::string make_error_response(int status_code, const std::string &status_text);

    static int get_content_length(const std::string& request);
};

#endif // HTTP_PARSER_HPP
