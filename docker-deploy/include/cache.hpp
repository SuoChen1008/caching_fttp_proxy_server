//
// Created by rs590 on 2/19/23.
//

#ifndef CACHE_HPP
#define CACHE_HPP

#include <unordered_map>
#include <string>
#include <mutex>
#include <list>
#include <iostream>
#include <memory>
#include <sstream>

#include "logger.hpp"
#include "http_parser.hpp"

class Logger;

class CacheEntry {
public:
    CacheEntry(const std::string& url, const std::string& response, bool must_revalidate,
               bool never_expires, bool no_cache, std::chrono::seconds max_age);

    const std::string& getUrl() const { return m_url; }
    const std::string& getResponse() const { return m_response; }
    bool isMustRevalidate() const { return m_must_revalidate; }
    bool isNeverExpires() const { return m_never_expires; }
    bool isNoCache() const { return m_no_cache; }
    std::chrono::system_clock::time_point getExpireTime() const { return m_expire_time; }

    bool isExpired() const;
    bool isFresh() const;

private:
    std::string m_url;
    std::string m_response;
    bool m_must_revalidate;
    bool m_never_expires;
    bool m_no_cache;
    std::chrono::system_clock::time_point m_expire_time;
};

class Cache {
public:
    static Cache& getInstance(Logger &logger);

    std::shared_ptr<CacheEntry> insert(const std::string &id, const std::string& url, const std::string& response);

    bool get(const std::string& url, std::shared_ptr<CacheEntry>& entry);

private:
    Cache(Logger &logger);

    void evict();

    std::list<CacheEntry> m_entries;
    std::unordered_map<std::string, std::list<CacheEntry>::iterator> m_entry_map;
    std::mutex m_mutex;
    Logger &m_logger;
};

#endif // CACHE_HPP
