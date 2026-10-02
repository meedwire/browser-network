#pragma once

#include "../../ports/INetworkAdapter.h"
#include "../../core/cache/HttpCache.h"
#include <memory>

namespace browser_network {

/**
 * @brief Decorator que adiciona suporte a Cache HTTP a um adaptador de rede.
 *
 * Intercepta chamadas send() para verificar o cache antes de ir à rede.
 * Gerencia revalidação condicional (304) e armazenamento de novas respostas.
 */
class CachedNetworkAdapter : public INetworkAdapter {
public:
    CachedNetworkAdapter(std::shared_ptr<INetworkAdapter> inner, 
                         std::shared_ptr<HttpCache> cache)
        : inner_(std::move(inner)), cache_(std::move(cache)) {}

    /**
     * @brief Envia requisição com lógica de cache.
     */
    std::future<HttpResponse> send(const HttpRequest& request) override;

    /**
     * @brief Envia requisição streaming (bypass de cache por enquanto).
     */
    std::future<StreamingResponse> sendStreaming(const HttpRequest& request) override;

private:
    std::shared_ptr<INetworkAdapter> inner_;
    std::shared_ptr<HttpCache> cache_;
};

} // namespace browser_network
