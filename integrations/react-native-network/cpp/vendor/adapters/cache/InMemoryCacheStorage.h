#pragma once

#include "../../ports/ICacheStorage.h"
#include <absl/container/flat_hash_map.h>
#include <list>
#include <mutex>
#include <string>

namespace browser_network {

/**
 * @brief Implementação de cache em memória com política de expulsão LRU.
 *
 * Utiliza absl::flat_hash_map para lookup O(1) e std::list para ordem de uso.
 * Thread-safe através de std::mutex.
 */
class InMemoryCacheStorage : public ICacheStorage {
public:
    explicit InMemoryCacheStorage(size_t maxEntries = 100) : maxEntries_(maxEntries) {}

    std::optional<CacheEntry> get(const std::string& key) override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = cache_.find(key);
        if (it == cache_.end()) {
            return std::nullopt;
        }

        // LRU: Move para o início da lista (mais recentemente usado)
        lru_.splice(lru_.begin(), lru_, it->second);
        return it->second->second;
    }

    void put(const std::string& key, const CacheEntry& entry) override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Se já existe, remove para reinserir no topo (atualização)
        auto it = cache_.find(key);
        if (it != cache_.end()) {
            lru_.erase(it->second);
            cache_.erase(it);
        }

        // Insere no início da lista
        lru_.push_front({key, entry});
        cache_[key] = lru_.begin();

        // Expulsão se exceder limite
        if (cache_.size() > maxEntries_) {
            auto last = lru_.end();
            last--; // O último é o menos recentemente usado
            cache_.erase(last->first);
            lru_.pop_back();
        }
    }

    void remove(const std::string& key) override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = cache_.find(key);
        if (it != cache_.end()) {
            lru_.erase(it->second);
            cache_.erase(it);
        }
    }

    void clear() override {
        std::lock_guard<std::mutex> lock(mutex_);
        cache_.clear();
        lru_.clear();
    }

private:
    size_t maxEntries_;
    // Lista duplamente encadeada armazenando pares <Key, Entry>
    std::list<std::pair<std::string, CacheEntry>> lru_;
    // Map apontando para iteradores da lista para acesso O(1)
    absl::flat_hash_map<std::string, std::list<std::pair<std::string, CacheEntry>>::iterator> cache_;
    std::mutex mutex_;
};

} // namespace browser_network
