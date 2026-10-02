#pragma once

#include "Origin.h"
#include "../HttpEntities.h"
#include "../HttpRequest.h"
#include <string>
#include <vector>

namespace browser_network {

/**
 * @brief Resultado da validação CORS.
 *
 * Indica se a resposta passou na validação e, caso negativo,
 * fornece a razão do bloqueio.
 */
struct CorsValidationResult {
    bool allowed = false;
    std::string blockReason;
};

/**
 * @brief Resultado da validação de preflight.
 */
struct PreflightResult {
    bool allowed = false;
    std::string blockReason;
    long maxAge = 0;  ///< Segundos para cachear o resultado (Access-Control-Max-Age)
};

/**
 * @brief Política CORS (Cross-Origin Resource Sharing).
 *
 * Implementa as regras do Fetch Standard para validação de CORS:
 *   - Detecção de requests simples vs complexos
 *   - Validação de headers de resposta CORS
 *   - Geração de requests de preflight (OPTIONS)
 *   - Validação de respostas de preflight
 *
 * Referência: https://fetch.spec.whatwg.org/#http-cors-protocol
 */
class CorsPolicy {
public:
    /**
     * @brief Verifica se um request é "simples" (não requer preflight).
     *
     * Um request é simples se:
     *   - Método é GET, HEAD ou POST
     *   - Apenas headers "simples" (CORS-safelisted)
     *   - Content-Type é um dos tipos simples
     *
     * @param method Método HTTP.
     * @param headers Headers da requisição.
     * @return true se não requer preflight.
     */
    static bool isSimpleRequest(HttpMethod method, const HeaderMap& headers);

    /**
     * @brief Verifica se um preflight é necessário.
     *
     * Inverso de isSimpleRequest, com verificações adicionais.
     *
     * @param request A requisição a verificar.
     * @return true se preflight OPTIONS deve ser enviado antes.
     */
    static bool isPreflightRequired(const HttpRequest& request);

    /**
     * @brief Valida a resposta CORS de um request normal.
     *
     * Verifica:
     *   - Access-Control-Allow-Origin match
     *   - Se credentials mode, wildcard (*) é rejeitado
     *
     * @param requestOrigin Origin do request.
     * @param responseHeaders Headers da resposta.
     * @param withCredentials Se o request inclui credentials.
     * @return Resultado da validação.
     */
    static CorsValidationResult validateResponse(
        const Origin& requestOrigin,
        const HeaderMap& responseHeaders,
        bool withCredentials = false);

    /**
     * @brief Valida a resposta de um preflight (OPTIONS).
     *
     * Verifica:
     *   - Access-Control-Allow-Origin
     *   - Access-Control-Allow-Methods
     *   - Access-Control-Allow-Headers
     *   - Access-Control-Max-Age
     *
     * @param requestOrigin Origin do request.
     * @param requestMethod Método do request original.
     * @param requestHeaders Headers customizados do request original.
     * @param preflightResponseHeaders Headers da resposta OPTIONS.
     * @return Resultado da validação do preflight.
     */
    static PreflightResult validatePreflight(
        const Origin& requestOrigin,
        HttpMethod requestMethod,
        const HeaderMap& requestHeaders,
        const HeaderMap& preflightResponseHeaders);

    /**
     * @brief Lista de headers CORS-safelisted (simples).
     *
     * Headers que não disparam preflight.
     */
    static const std::vector<std::string>& safelistedHeaders();

    /**
     * @brief Lista de Content-Types simples.
     *
     * Content-Types que não disparam preflight em POST.
     */
    static const std::vector<std::string>& simpleContentTypes();

    /**
     * @brief Converte HttpMethod para string.
     */
    static std::string methodToString(HttpMethod method);
};

} // namespace browser_network
