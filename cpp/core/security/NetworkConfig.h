#pragma once

#include <string>

namespace browser_network {

/**
 * @brief Modo de operação do engine de rede.
 *
 * Native: Engine pura de rede sem regras de navegador.
 *   - Nenhum header Origin emitido
 *   - Nenhuma regra de CORS
 *   - Nenhuma filtragem de cookies
 *
 * BrowserEmulation: Simula comportamento de navegador.
 *   - Origin sintética sempre presente
 *   - CORS enforced
 *   - Cookie security (Secure, SameSite) enforced
 *   - Preflight automático
 */
enum class OriginMode {
    Native,
    BrowserEmulation
};

/**
 * @brief Configuração de segurança do engine de rede.
 *
 * Controla o modo de operação e a origem utilizada nas requisições.
 *
 * Prioridade de resolução da origin:
 *   1. customOrigin (se não vazio)
 *   2. debugLocalhost (se true → http://localhost)
 *   3. syntheticOrigin derivada do bundleId
 *
 * Invariantes:
 *   - Mesmo código → mesmo comportamento
 *   - dev ≈ prod
 *   - Origin sempre válida
 *   - Secure context previsível
 */
struct NetworkConfig {
    OriginMode mode = OriginMode::Native;

    /**
     * @brief ID do bundle do app (ex: "com.empresa.app").
     * Usado para derivar a origin sintética em BrowserEmulation.
     */
    std::string bundleId;

    /**
     * @brief Modo debug: usa http://localhost como origin.
     * localhost é tratado como secure context mesmo em HTTP.
     */
    bool debugLocalhost = false;

    /**
     * @brief Override explícito da origin (maior prioridade).
     * Se não vazio, sobrescreve bundleId e debugLocalhost.
     * Formato: "https://custom.origin.com"
     */
    std::string customOrigin;

    /**
     * @brief Caminho do arquivo usado para persistir cookies.
     *
     * Se vazio, os cookies persistentes ficam apenas em memória.
     */
    std::string cookiePersistencePath;
};

} // namespace browser_network
