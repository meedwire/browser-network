#pragma once

#include "CachePolicy.h"
#include "../../ports/ICacheStorage.h"
#include "../HttpRequest.h"
#include "../HttpResponse.h"
#include <memory>

namespace browser_network {

/**
 * @brief Orquestrador de Cache HTTP.
 * 
 * Responsável por:
 * 1. Consultar o armazenamento (ICacheStorage).
 * 2. Aplicar a política de validação (CachePolicy).
 * 3. Decidir se deve usar cache, revalidar ou buscar da rede.
 * 4. Atualizar o cache com novas respostas.
 */
class HttpCache {
public:
    explicit HttpCache(std::shared_ptr<ICacheStorage> storage) 
        : storage_(std::move(storage)) {}

    struct Resolution {
        CacheResponseStatus status;
        std::optional<CacheEntry> entry;
    };

    /**
     * @brief Resolve uma requisição contra o cache.
     */
    Resolution resolve(const HttpRequest& req);

    /**
     * @brief Armazena ou atualiza uma resposta no cache.
     */
    void store(const HttpRequest& req, const HttpResponse& res, 
               const std::optional<CacheEntry>& staleEntry = std::nullopt);

    /**
     * @brief Atualiza metadados após uma revalidação 304.
     */
    void update(const std::string& key, const CacheEntry& staleEntry, const HttpResponse& validationRes);

private:
    std::shared_ptr<ICacheStorage> storage_;
};

} // namespace browser_network
