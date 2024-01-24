//
// Created by Rocco Su on 2/16/23.
//

#include "cache.hpp"

#include "http_parser.hpp"

CacheEntry::CacheEntry(const std::string &url, const std::string &response, bool must_revalidate,
                       bool never_expires, bool no_cache, std::chrono::seconds max_age)
        : m_url(url),
          m_response(response),
          m_must_revalidate(must_revalidate),
          m_never_expires(never_expires),
          m_no_cache(no_cache),
          m_expire_time(std::chrono::system_clock::now() + max_age) {}

bool CacheEntry::isExpired() const {
    if (m_never_expires) {
        return false;
    }
    if (m_must_revalidate) {
        return true;
    }
    if (m_no_cache) {
        return true;
    }
    auto now = std::chrono::system_clock::now();
    return now > m_expire_time;
}

bool CacheEntry::isFresh() const {
    if (m_never_expires) {
        return true;
    }
    std::chrono::system_clock::time_point current_time = std::chrono::system_clock::now();
    return current_time < m_expire_time;
}

Cache &Cache::getInstance(Logger &logger) {
    static Cache instance(logger);
    return instance;
}

Cache::Cache(Logger &logger) : m_logger(logger) {}

std::shared_ptr<CacheEntry> Cache::insert(const std::string &id, const std::string &url, const std::string &response) {
    auto headers = HTTP_Parser::parse_headers(response);
    auto cache_control = HTTP_Parser::parse_cache_control(headers["Cache-Control"]);
    auto max_age_str = cache_control["max-age"];
    auto must_revalidate = cache_control.count("must-revalidate") > 0;
    auto no_cache = cache_control.count("no-cache") > 0;
    auto max_age = std::chrono::seconds(max_age_str.empty() ? 0 : std::stoi(max_age_str));
    std::string etag = HTTP_Parser::get_header_value("ETag", headers);
    std::string cache_control_str = HTTP_Parser::get_header_value("Cache-Control", headers);
    auto never_expires = cache_control_str.empty() ? true : false;

    if (!etag.empty()) {
        std::string message = "ETag: " + etag;
        m_logger.note_with_id(id, message);
    }

    if (!cache_control_str.empty()) {
        std::string message = "Cache-Control: " + cache_control_str;
        m_logger.note_with_id(id, message);
    }

    std::unique_lock<std::mutex> lock(m_mutex);

    // Check if the entry already exists in the cache
    auto it = m_entry_map.find(url);
    if (it != m_entry_map.end()) {
        // Update the existing entry
        auto &entry = *(it->second);
        entry = CacheEntry(url, response, must_revalidate, never_expires, no_cache, max_age);
        // Move the entry to the front of the list
        m_entries.splice(m_entries.begin(), m_entries, it->second);
    } else {
        // Create a new entry
        m_entries.emplace_front(url, response, must_revalidate, never_expires, no_cache, max_age);
        // Add the new entry to the map
        m_entry_map[url] = m_entries.begin();

        // Check if the cache size exceeds the limit
        if (m_entries.size() > 10240) {
            evict();
        }
    }

// Return a shared pointer to the inserted cache entry
    return std::make_shared<CacheEntry>(*m_entry_map[url]);
}


bool Cache::get(const std::string &url, std::shared_ptr<CacheEntry> &entry) {
    std::unique_lock<std::mutex> lock(m_mutex);

    auto it = m_entry_map.find(url);
    if (it != m_entry_map.end()) {
        auto &cacheEntry = *(it->second);
        // Move the entry to the front of the list
        m_entries.splice(m_entries.begin(), m_entries, it->second);

        entry = std::make_shared<CacheEntry>(cacheEntry);
        return true;
    }

    return false;
}

void Cache::evict() {
    if (!m_entries.empty()) {
        // Remove the least recently used entry from the cache
        auto &entry = m_entries.back();

        std::string response, host;
        response = entry.getResponse();
        host = entry.getUrl();
        m_logger.note("evicted " + host + " from cache");

        m_entry_map.erase(entry.getUrl());
        m_entries.pop_back();
    }
}
