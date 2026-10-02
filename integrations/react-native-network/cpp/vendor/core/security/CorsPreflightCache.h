#pragma once

#include "Origin.h"
#include "../HttpEntities.h"
#include "absl/container/flat_hash_map.h"
#include <string>
#include <chrono>
#include <mutex>

namespace browser_network {

/**
 * @brief Cache de respostas de preflight CORS.
 *
 * Armazena resultados de preflight (OPTIONS) para evitar envio repetido.
 * Cada entrada tem um TTL baseado no header Access-Control-Max-Age.
 *
 * A chave do cache é: origin_serializada + ":" + url + ":" + método.
 * Thread-safe via mutex.
 *
 * Referência: Fetch Standard §3.2.3 (CORS-preflight cache)
 */
class CorsPreflightCache {
public:
    using TimePoint = std::chrono::system_clock::time_point;

    /**
     * @brief Entrada no cache de preflight.
     */
    struct Entry {
        std::string allowedMethods;   ///< Access-Control-Allow-Methods cacheado
        std::string allowedHeaders;   ///< Access-Control-Allow-Headers cacheado
        TimePoint expiresAt;          ///< Momento de expiração
    };

    /**
     * @brief Verifica se existe um preflight cacheado e válido.
     *
     * @param origin Origin do request.
     * @param url URL do request.
     * @param method Método do request.
     * @return true se há cache válido (não expirado).
     */
    bool hasValidEntry(const Origin& origin, const std::string& url, const std::string& method) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::string key = makeKey(origin, url, method);
        auto it = cache_.find(key);
        if (it == cache_.end()) return false;

        auto now = std::chrono::system_clock::now();
        if (now > it->second.expiresAt) {
            cache_.erase(it);
            return false;
        }
        return true;
    }

    /**
     * @brief Armazena resultado de preflight no cache.
     *
     * @param origin Origin do request.
     * @param url URL do request.
     * @param method Método do request.
     * @param allowedMethods Valor de Access-Control-Allow-Methods.
     * @param allowedHeaders Valor de Access-Control-Allow-Headers.
     * @param maxAgeSeconds TTL em segundos (Access-Control-Max-Age).
     */
    void store(const Origin& origin, const std::string& url, const std::string& method,
               const std::string& allowedMethods, const std::string& allowedHeaders,
               long maxAgeSeconds) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::string key = makeKey(origin, url, method);

        Entry entry;
        entry.allowedMethods = allowedMethods;
        entry.allowedHeaders = allowedHeaders;
        entry.expiresAt = std::chrono::system_clock::now() + std::chrono::seconds(maxAgeSeconds);

        cache_[key] = std::move(entry);
    }

    /**
     * @brief Limpa todo o cache de preflight.
     */
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        cache_.clear();
    }

private:
    /**
     * @brief Gera chave composta para o cache.
     */
    static std::string makeKey(const Origin& origin, const std::string& url, const std::string& method) {
        return origin.serialize() + ":" + url + ":" + method;
    }

    absl::flat_hash_map<std::string, Entry> cache_;
    std::mutex mutex_;
};

} // namespace browser_network
