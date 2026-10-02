#pragma once

#include "../core/cache/CacheEntry.h"
#include <string>
#include <optional>

namespace browser_network {

/**
 * @brief Interface abstrata para armazenamento de cache.
 *
 * Permite backends plugáveis (Memória, Disco, SQLite, etc.).
 * Segue o princípio de Inversão de Dependência (DIP).
 */
class ICacheStorage {
public:
    virtual ~ICacheStorage() = default;
    
    /**
     * @brief Recupera uma entrada do cache.
     * @param key A chave única (geralmente URL).
     * @return A entrada se existir, ou std::nullopt.
     */
    virtual std::optional<CacheEntry> get(const std::string& key) = 0;
    
    /**
     * @brief Armazena uma entrada no cache.
     * @param key A chave única.
     * @param entry A entrada completa a ser salva.
     */
    virtual void put(const std::string& key, const CacheEntry& entry) = 0;
    
    /**
     * @brief Remove uma entrada específica.
     * @param key A chave a ser removida.
     */
    virtual void remove(const std::string& key) = 0;
    
    /**
     * @brief Limpa todas as entradas (purge).
     */
    virtual void clear() = 0;
};

} // namespace browser_network
