#include "SecurityPolicy.h"
#include <algorithm>
#include <cctype>

namespace browser_network {

Origin SecurityPolicy::deriveSyntheticOrigin(const std::string& bundleId) {
    Origin origin;
    origin.scheme = "https";
    origin.port = 443;

    /**
     * Transformação do bundleId em hostname válido:
     *   1. Converter para lowercase
     *   2. Substituir '.' por '-'
     *   3. Remover caracteres inválidos para hostname (RFC 3986)
     *   4. Adicionar sufixo ".app"
     *
     * Exemplo: "com.Empresa.App" → "com-empresa-app.app"
     */
    std::string host;
    host.reserve(bundleId.size());

    for (char c : bundleId) {
        char lower = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (lower == '.') {
            host += '-';
        } else if (std::isalnum(static_cast<unsigned char>(lower)) || lower == '-') {
            host += lower;
        }
        // Caracteres inválidos são silenciosamente removidos
    }

    // Remover hífens duplicados consecutivos
    std::string cleaned;
    cleaned.reserve(host.size());
    for (size_t i = 0; i < host.size(); ++i) {
        if (host[i] == '-' && !cleaned.empty() && cleaned.back() == '-') {
            continue;
        }
        cleaned += host[i];
    }

    // Remover hífens no início e fim
    while (!cleaned.empty() && cleaned.front() == '-') cleaned.erase(0, 1);
    while (!cleaned.empty() && cleaned.back() == '-') cleaned.pop_back();

    origin.host = cleaned + ".app";
    return origin;
}

bool SecurityPolicy::isSecureContext(const Origin& origin) {
    /**
     * W3C Secure Contexts (https://w3c.github.io/webappsec-secure-contexts/):
     *   - HTTPS é sempre seguro
     *   - localhost (e variantes) é seguro mesmo em HTTP
     *   - Qualquer outro HTTP é inseguro
     */
    if (origin.scheme == "https") return true;
    if (origin.isLocalhost()) return true;
    return false;
}

Origin SecurityPolicy::resolveOrigin(const NetworkConfig& config) {
    /**
     * Resolução determinística da origin:
     *   Prioridade: customOrigin > debugLocalhost > syntheticOrigin
     *   Nada implícito. Tudo previsível.
     */

    // 1. Custom origin (maior prioridade)
    if (!config.customOrigin.empty()) {
        auto parsed = Origin::fromUrl(config.customOrigin);
        if (parsed.has_value()) {
            return parsed.value();
        }
    }

    // 2. Debug localhost
    if (config.debugLocalhost) {
        Origin debug;
        debug.scheme = "http";
        debug.host = "localhost";
        debug.port = 80;
        return debug;
    }

    // 3. Synthetic origin derivada do bundleId
    return deriveSyntheticOrigin(config.bundleId);
}

bool SecurityPolicy::isSameOrigin(const Origin& a, const Origin& b) {
    return a == b;
}

} // namespace browser_network
