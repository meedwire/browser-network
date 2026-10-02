#pragma once

#include "../cache/CacheEntry.h"
#include "../HttpRequest.h"
#include "../HttpResponse.h"
#include <string>
#include <vector>

namespace browser_network {

enum class CacheResponseStatus {
    Fresh,             ///< Pode ser servido do cache
    Stale,             ///< Precisa de revalidação (síncrona)
    StaleWhileRevalidate, ///< Pode ser servido, mas requer revalidação em background
    StaleIfError,      ///< Pode ser servido se houver erro de rede
    Miss,              ///< Não encontrado ou não elegível
    Immutable          ///< Pode ser servido indefinidamente (extensão)
};

class CachePolicy {
public:
    /**
     * @brief Determina se uma resposta pode ser armazenada (RFC 7234 Seção 3).
     */
    static bool isCacheable(const HttpRequest& req, const HttpResponse& res);

    /**
     * @brief Calcula a idade atual de uma entrada (RFC 7234 Seção 4.2.3).
     */
    static std::chrono::seconds computeAge(const CacheEntry& entry, TimePoint now);

    /**
     * @brief Calcula o tempo de frescor (freshness lifetime).
     */
    static std::chrono::seconds computeFreshnessLifetime(const CacheEntry& entry);

    /**
     * @brief Verifica o estado de uma entrada para uma nova requisição.
     */
    static CacheResponseStatus checkStatus(const CacheEntry& entry, const HttpRequest& req, TimePoint now);

    /**
     * @brief Verifica se a requisição pode ser servida por esta entrada (Vary check).
     */
    static bool matchesRequest(const CacheEntry& entry, const HttpRequest& req);

    // Configuração de heurística
    static void setHeuristicFraction(float fraction) { heuristicFraction_ = fraction; }

private:
    static float heuristicFraction_; // Padrão 0.1
};

} // namespace browser_network
