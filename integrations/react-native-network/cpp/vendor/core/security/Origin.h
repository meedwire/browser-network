#pragma once

#include <string>
#include <optional>

namespace browser_network {

/**
 * @brief Representa uma origem (origin) conforme RFC 6454.
 *
 * Uma origin é composta por scheme + host + port. No modo BrowserEmulation,
 * toda requisição possui uma origin sintética derivada do bundleId do app.
 *
 * Exemplos:
 *   - https://com-empresa-app.app (sintética)
 *   - http://localhost (debug)
 *   - https://api.example.com:8080 (external)
 */
struct Origin {
    std::string scheme;  ///< Ex: "https", "http"
    std::string host;    ///< Ex: "com-empresa-app.app", "localhost"
    int port = 0;        ///< 0 = porta padrão do scheme (80/443)
    bool isOpaque = false; ///< Nunca geramos opaque origins

    /**
     * @brief Serializa a origin conforme RFC 6454.
     *
     * Formato: scheme://host[:port]
     * A porta é omitida se for a padrão do scheme (80 para http, 443 para https).
     *
     * @return String serializada da origin.
     */
    std::string serialize() const {
        std::string result = scheme + "://" + host;
        bool isDefaultPort = (port == 0) ||
                             (scheme == "https" && port == 443) ||
                             (scheme == "http" && port == 80);
        if (!isDefaultPort) {
            result += ":" + std::to_string(port);
        }
        return result;
    }

    bool operator==(const Origin& other) const {
        return scheme == other.scheme &&
               host == other.host &&
               effectivePort() == other.effectivePort();
    }

    bool operator!=(const Origin& other) const {
        return !(*this == other);
    }

    /**
     * @brief Retorna a porta efetiva (resolve 0 para a padrão do scheme).
     */
    int effectivePort() const {
        if (port != 0) return port;
        if (scheme == "https") return 443;
        if (scheme == "http") return 80;
        return 0;
    }

    /**
     * @brief Verifica se a origin é localhost.
     */
    bool isLocalhost() const {
        return host == "localhost" || host == "127.0.0.1" || host == "::1";
    }

    /**
     * @brief Extrai uma Origin a partir de uma URL completa.
     * @param url URL absoluta (ex: "https://api.example.com:8080/path")
     * @return Origin extraída, ou nullopt se URL inválida.
     */
    static std::optional<Origin> fromUrl(const std::string& url) {
        size_t schemeEnd = url.find("://");
        if (schemeEnd == std::string::npos) return std::nullopt;

        Origin o;
        o.scheme = url.substr(0, schemeEnd);

        size_t hostStart = schemeEnd + 3;
        size_t hostEnd = url.find('/', hostStart);
        std::string authority = (hostEnd != std::string::npos)
            ? url.substr(hostStart, hostEnd - hostStart)
            : url.substr(hostStart);

        // Verificar se há porta explícita
        size_t colonPos = authority.rfind(':');
        // Distinguir de IPv6 (ex: [::1]:8080)
        size_t bracketPos = authority.rfind(']');
        if (colonPos != std::string::npos &&
            (bracketPos == std::string::npos || colonPos > bracketPos)) {
            o.host = authority.substr(0, colonPos);
            try {
                o.port = std::stoi(authority.substr(colonPos + 1));
            } catch (...) {
                o.port = 0;
            }
        } else {
            o.host = authority;
            o.port = 0;
        }

        return o;
    }
};

} // namespace browser_network
