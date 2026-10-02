#pragma once

#include "../HttpEntities.h"
#include <chrono>
#include <string>
#include <optional>

namespace browser_network {

using Clock = std::chrono::system_clock;
using TimePoint = std::chrono::time_point<Clock>;

/**
 * @brief Metadados necessários para cálculo de Freshness e Age (RFC 7234).
 */
struct CacheMetadata {
    TimePoint storedAt;       ///< Data de armazenamento (local)
    TimePoint requestTime;    ///< Data de envio da requisição (local)
    TimePoint responseTime;   ///< Data de recebimento da resposta (local)
    
    // Headers pré-processados para acesso rápido
    std::optional<TimePoint> dateValue;          ///< Parse do header Date
    std::optional<TimePoint> expiresValue;       ///< Parse do header Expires
    std::optional<TimePoint> lastModifiedValue;  ///< Parse do header Last-Modified
    
    std::string etag;         ///< Valor do ETag
    std::string vary;         ///< Valor do Vary
    std::string cacheControl; ///< Valor do Cache-Control
    long ageValue = 0;        ///< Valor numérico do header Age, se presente
};

/**
 * @brief Entrada completa armazenada no cache.
 *
 * Contém a resposta, headers de variação e metadados de controle.
 */
struct CacheEntry {
    std::string url;
    HttpMethod method;
    int status;
    std::string statusMessage;
    
    HeaderMap responseHeaders;
    HeaderMap originalRequestHeaders; ///< Headers da requisição original (para Vary check)
    
    ByteBuffer responseBody;
    
    CacheMetadata metadata;
};

} // namespace browser_network
