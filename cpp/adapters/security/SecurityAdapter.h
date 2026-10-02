#pragma once

#include "../../ports/INetworkAdapter.h"
#include "../../core/security/Origin.h"
#include "../../core/security/NetworkConfig.h"
#include "../../core/security/CookieJar.h"
#include "../../core/security/SecurityPolicy.h"
#include "../../core/security/CookiePolicy.h"
#include "../../core/security/CorsPolicy.h"
#include "../../core/security/CorsPreflightCache.h"
#include <memory>
#include <stdexcept>

namespace browser_network {

/**
 * @brief Exceção lançada quando CORS bloqueia uma resposta.
 *
 * Emula o comportamento de navegador: mesmo que o servidor responda 200,
 * se os headers CORS não forem válidos, a resposta é bloqueada.
 */
class CorsBlockedError : public std::runtime_error {
public:
    explicit CorsBlockedError(const std::string& reason)
        : std::runtime_error("CORS Blocked: " + reason) {}
};

/**
 * @brief Decorator que adiciona segurança de navegador ao engine de rede.
 *
 * Implementa INetworkAdapter e envelopa outro adapter (inner).
 * Em modo Native, é 100% pass-through (zero overhead).
 * Em modo BrowserEmulation, aplica:
 *   - Emissão de header Origin
 *   - Preflight automático (OPTIONS) para requests complexos
 *   - Validação CORS na resposta
 *   - Filtragem de cookies inseguros (Set-Cookie enforcement)
 *
 * Composição recomendada:
 *   SecurityAdapter → CachedNetworkAdapter → CurlAdapter
 */
class SecurityAdapter : public INetworkAdapter {
public:
    /**
     * @brief Constrói o SecurityAdapter.
     *
     * @param inner Adapter de rede subjacente (ex: CurlAdapter ou CachedNetworkAdapter).
     * @param config Configuração de segurança.
     */
    SecurityAdapter(
        std::shared_ptr<INetworkAdapter> inner,
        NetworkConfig config,
        std::shared_ptr<CookieJar> cookieJar = nullptr
    );

    /**
     * @brief Envia uma requisição com enforcement de segurança.
     *
     * Fluxo em BrowserEmulation:
     *   1. Resolver origin efetiva
     *   2. Adicionar header Origin
     *   3. Se preflight necessário: enviar OPTIONS, validar, cachear
     *   4. Enviar request real
     *   5. Validar CORS na resposta
     *   6. Filtrar cookies inseguros
     *   7. Retornar resposta (ou throw CorsBlockedError)
     */
    std::future<HttpResponse> send(const HttpRequest& request) override;

    /**
     * @brief Streaming com enforcement de segurança.
     *
     * Em BrowserEmulation, aplica as mesmas regras de Origin e CORS.
     * Em Native, pass-through direto.
     */
    std::future<StreamingResponse> sendStreaming(const HttpRequest& request) override;

    std::shared_ptr<CookieJar> getCookieJar() const { return cookieJar_; }

private:
    std::shared_ptr<INetworkAdapter> inner_;
    NetworkConfig config_;
    Origin effectiveOrigin_;
    CorsPreflightCache preflightCache_;
    std::shared_ptr<CookieJar> cookieJar_;

    /**
     * @brief Executa preflight OPTIONS e valida resposta.
     * @throws CorsBlockedError se preflight falhar.
     */
    void performPreflight(const HttpRequest& request);

    /**
     * @brief Valida CORS na resposta.
     * @throws CorsBlockedError se validação falhar.
     */
    void validateCors(const HttpResponse& response, bool withCredentials);

    /**
     * @brief Aplica enforcement de cookies na resposta.
     */
    void enforceCookieSecurity(HttpResponse& response);
    void enforceCookieSecurity(StreamingResponse& response);
};

} // namespace browser_network
