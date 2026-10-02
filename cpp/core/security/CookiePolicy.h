#pragma once

#include "Origin.h"
#include "SecurityPolicy.h"
#include "../HttpEntities.h"
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace browser_network {

/**
 * @brief Atributos parseados de um header Set-Cookie.
 *
 * Representa os campos relevantes para enforcement de segurança,
 * conforme RFC 6265bis (draft) e comportamento de navegadores modernos.
 */
struct CookieAttributes {
    std::string name;
    std::string value;
    std::string domain;
    std::string path;
    bool secure = false;          ///< Flag Secure presente
    bool httpOnly = false;        ///< Flag HttpOnly presente
    std::string sameSite;         ///< "Strict", "Lax", "None", ou vazio
    bool domainSpecified = false;
    bool pathSpecified = false;
    std::optional<int64_t> maxAgeSeconds;
    std::optional<int64_t> expiresUnixSeconds;

    /**
     * @brief Verifica se o cookie exige secure context.
     *
     * Um cookie exige secure context se:
     *   - Possui flag Secure, ou
     *   - SameSite=None (que requer Secure em navegadores modernos)
     */
    bool requiresSecureContext() const {
        return secure || sameSite == "None";
    }
};

/**
 * @brief Política de segurança de cookies.
 *
 * Implementa as regras de navegador moderno para aceitação/rejeição de cookies:
 *   - Cookie com Secure → rejeitado se !isSecureContext
 *   - Cookie com SameSite=None → requer Secure flag + secure context
 *   - Filtragem de headers Set-Cookie em respostas
 */
class CookiePolicy {
public:
    /**
     * @brief Parseia um header Set-Cookie em CookieAttributes.
     *
     * Extrai nome, valor e atributos (Secure, HttpOnly, SameSite, Domain, Path).
     *
     * @param setCookieHeader Valor completo do header Set-Cookie.
     * @return Atributos parseados, ou nullopt se inválido.
     */
    static std::optional<CookieAttributes> parseSetCookie(const std::string& setCookieHeader);

    /**
     * @brief Valida se um cookie deve ser aceito no contexto atual.
     *
     * Regras:
     *   1. Cookie com Secure flag → requer secure context
     *   2. Cookie com SameSite=None → requer Secure flag
     *   3. Cookie com SameSite=None sem Secure → rejeitado
     *
     * @param cookie Atributos do cookie a validar.
     * @param requestOrigin Origin da requisição.
     * @return true se o cookie deve ser aceito.
     */
    static bool shouldAcceptCookie(const CookieAttributes& cookie, const Origin& requestOrigin);

    /**
     * @brief Filtra headers Set-Cookie de uma resposta.
     *
     * Remove cookies que não passam na validação de segurança.
     * Modifica os headers in-place.
     *
     * @param responseHeaders Headers da resposta (modificados in-place).
     * @param requestOrigin Origin da requisição atual.
     * @return Número de cookies removidos.
     */
    static int filterResponseCookies(HeaderMap& responseHeaders, const Origin& requestOrigin);

    /**
     * @brief Filtra múltiplos headers Set-Cookie.
     *
     * Remove entradas inválidas ou rejeitadas conforme as regras de segurança.
     *
     * @param setCookieHeaders Vetor com valores individuais de Set-Cookie.
     * @param requestOrigin Origin da requisição atual.
     * @return Número de cookies removidos.
     */
    static int filterResponseCookies(std::vector<std::string>& setCookieHeaders,
                                     const Origin& requestOrigin);
};

} // namespace browser_network
