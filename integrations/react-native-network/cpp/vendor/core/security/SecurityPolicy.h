#pragma once

#include "Origin.h"
#include "NetworkConfig.h"
#include <string>

namespace browser_network {

/**
 * @brief Política de segurança do engine — pure functions.
 *
 * Centraliza todas as regras de segurança relacionadas a origins e
 * secure contexts. Todas as funções são estáticas e determinísticas.
 *
 * Responsabilidades:
 *   - Derivar origin sintética a partir de bundleId
 *   - Classificar secure context
 *   - Resolver origin efetiva com base no NetworkConfig
 */
class SecurityPolicy {
public:
    /**
     * @brief Deriva uma origin sintética a partir do bundleId do app.
     *
     * Transformação:
     *   com.empresa.app → https://com-empresa-app.app
     *
     * Regras:
     *   - lowercase
     *   - substituir '.' por '-'
     *   - remover caracteres inválidos (RFC 3986 hostname safe)
     *
     * @param bundleId Identificador do app (ex: "com.empresa.app")
     * @return Origin sintética.
     */
    static Origin deriveSyntheticOrigin(const std::string& bundleId);

    /**
     * @brief Verifica se uma origin constitui um "secure context".
     *
     * Regras (W3C Secure Contexts):
     *   - scheme == "https" → secure
     *   - host == "localhost" → secure (mesmo em HTTP)
     *   - host == "127.0.0.1" → secure
     *   - host == "::1" → secure
     *   - caso contrário → insecure
     *
     * @param origin A origin a verificar.
     * @return true se o contexto é seguro.
     */
    static bool isSecureContext(const Origin& origin);

    /**
     * @brief Resolve a origin efetiva com base na configuração.
     *
     * Prioridade:
     *   1. customOrigin (se não vazio)
     *   2. debugLocalhost (se true → http://localhost)
     *   3. syntheticOrigin derivada do bundleId
     *
     * @param config Configuração de rede.
     * @return Origin resolvida.
     */
    static Origin resolveOrigin(const NetworkConfig& config);

    /**
     * @brief Verifica se duas origins são same-origin.
     *
     * Same-origin = mesmo scheme + host + port efetivo.
     *
     * @param a Primeira origin.
     * @param b Segunda origin.
     * @return true se são same-origin.
     */
    static bool isSameOrigin(const Origin& a, const Origin& b);
};

} // namespace browser_network
