#include "CorsPolicy.h"
#include <algorithm>
#include <cctype>
#include <sstream>

namespace browser_network {

/**
 * @brief Converte string para lowercase (auxiliar local).
 */
static std::string corsToLower(const std::string& s) {
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}

/**
 * @brief Trim de whitespace (auxiliar local).
 */
static std::string corsTrim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

/**
 * @brief Obtém valor de header case-insensitive.
 */
static std::string getHeaderCI(const HeaderMap& headers, const std::string& key) {
    // Tentar exato
    auto it = headers.find(key);
    if (it != headers.end()) return it->second;

    // Tentar lowercase
    std::string lower = corsToLower(key);
    for (const auto& [k, v] : headers) {
        if (corsToLower(k) == lower) return v;
    }
    return "";
}

const std::vector<std::string>& CorsPolicy::safelistedHeaders() {
    /**
     * Headers CORS-safelisted que não disparam preflight.
     * Referência: Fetch Standard §2.2.2
     */
    static const std::vector<std::string> headers = {
        "accept",
        "accept-language",
        "content-language",
        "content-type"
    };
    return headers;
}

const std::vector<std::string>& CorsPolicy::simpleContentTypes() {
    /**
     * Content-Types que não disparam preflight em requests POST.
     * Referência: Fetch Standard §2.2.2
     */
    static const std::vector<std::string> types = {
        "application/x-www-form-urlencoded",
        "multipart/form-data",
        "text/plain"
    };
    return types;
}

std::string CorsPolicy::methodToString(HttpMethod method) {
    switch (method) {
        case HttpMethod::GET: return "GET";
        case HttpMethod::POST: return "POST";
        case HttpMethod::PUT: return "PUT";
        case HttpMethod::DELETE: return "DELETE";
        case HttpMethod::PATCH: return "PATCH";
        case HttpMethod::HEAD: return "HEAD";
        case HttpMethod::OPTIONS: return "OPTIONS";
    }
    return "GET";
}

bool CorsPolicy::isSimpleRequest(HttpMethod method, const HeaderMap& headers) {
    /**
     * Um request é "simples" se:
     *   1. Método é GET, HEAD ou POST
     *   2. Todos os headers são CORS-safelisted
     *   3. Se POST, Content-Type é um dos tipos simples
     */

    // Verificar método
    if (method != HttpMethod::GET &&
        method != HttpMethod::HEAD &&
        method != HttpMethod::POST) {
        return false;
    }

    const auto& safe = safelistedHeaders();

    // Verificar headers
    for (const auto& [key, value] : headers) {
        std::string lowerKey = corsToLower(key);
        bool isSafe = std::find(safe.begin(), safe.end(), lowerKey) != safe.end();
        if (!isSafe) {
            return false;
        }
    }

    // Para POST, verificar Content-Type
    if (method == HttpMethod::POST) {
        std::string ct = corsToLower(getHeaderCI(headers, "Content-Type"));
        // Extrair apenas o media type (antes de ;)
        size_t semicolon = ct.find(';');
        if (semicolon != std::string::npos) {
            ct = corsTrim(ct.substr(0, semicolon));
        }

        const auto& simpleTypes = simpleContentTypes();
        bool isSimpleCT = std::find(simpleTypes.begin(), simpleTypes.end(), ct) != simpleTypes.end();
        if (!ct.empty() && !isSimpleCT) {
            return false;
        }
    }

    return true;
}

bool CorsPolicy::isPreflightRequired(const HttpRequest& request) {
    return !isSimpleRequest(request.getMethod(), request.getHeaders());
}

CorsValidationResult CorsPolicy::validateResponse(
    const Origin& requestOrigin,
    const HeaderMap& responseHeaders,
    bool withCredentials) {

    CorsValidationResult result;

    std::string allowOrigin = getHeaderCI(responseHeaders, "Access-Control-Allow-Origin");

    if (allowOrigin.empty()) {
        result.allowed = false;
        result.blockReason = "Resposta sem header Access-Control-Allow-Origin";
        return result;
    }

    /**
     * Regra de wildcard:
     *   '*' é aceito APENAS se withCredentials == false.
     *   Com credentials, o servidor DEVE especificar a origin exata.
     */
    if (allowOrigin == "*") {
        if (withCredentials) {
            result.allowed = false;
            result.blockReason = "Wildcard (*) não permitido com credentials";
            return result;
        }
        result.allowed = true;
        return result;
    }

    // Comparar origin serializada
    if (allowOrigin == requestOrigin.serialize()) {
        // Verificar credentials se necessário
        if (withCredentials) {
            std::string allowCreds = getHeaderCI(responseHeaders, "Access-Control-Allow-Credentials");
            if (corsToLower(allowCreds) != "true") {
                result.allowed = false;
                result.blockReason = "Credentials exigidos mas Access-Control-Allow-Credentials != true";
                return result;
            }
        }
        result.allowed = true;
        return result;
    }

    result.allowed = false;
    result.blockReason = "Origin '" + requestOrigin.serialize() +
                         "' não permitida. Servidor respondeu: '" + allowOrigin + "'";
    return result;
}

PreflightResult CorsPolicy::validatePreflight(
    const Origin& requestOrigin,
    HttpMethod requestMethod,
    const HeaderMap& requestHeaders,
    const HeaderMap& preflightResponseHeaders) {

    PreflightResult result;

    // 1. Validar Allow-Origin
    auto corsResult = validateResponse(requestOrigin, preflightResponseHeaders, false);
    if (!corsResult.allowed) {
        result.allowed = false;
        result.blockReason = "Preflight: " + corsResult.blockReason;
        return result;
    }

    // 2. Validar Allow-Methods
    std::string allowMethods = getHeaderCI(preflightResponseHeaders, "Access-Control-Allow-Methods");
    std::string methodStr = methodToString(requestMethod);

    if (allowMethods != "*") {
        // Parsear lista separada por vírgula
        bool methodFound = false;
        std::istringstream stream(allowMethods);
        std::string m;
        while (std::getline(stream, m, ',')) {
            if (corsTrim(m) == methodStr) {
                methodFound = true;
                break;
            }
        }
        if (!methodFound) {
            result.allowed = false;
            result.blockReason = "Método '" + methodStr + "' não permitido pelo preflight";
            return result;
        }
    }

    // 3. Validar Allow-Headers
    std::string allowHeaders = getHeaderCI(preflightResponseHeaders, "Access-Control-Allow-Headers");
    const auto& safe = safelistedHeaders();

    if (allowHeaders != "*") {
        // Parsear lista de headers permitidos
        std::vector<std::string> allowed;
        std::istringstream hStream(allowHeaders);
        std::string h;
        while (std::getline(hStream, h, ',')) {
            allowed.push_back(corsToLower(corsTrim(h)));
        }

        // Verificar se todos os headers não-simples estão permitidos
        for (const auto& [key, value] : requestHeaders) {
            std::string lowerKey = corsToLower(key);
            bool isSafe = std::find(safe.begin(), safe.end(), lowerKey) != safe.end();
            // Headers do protocolo CORS não são "custom headers"
            bool isCorsProtocol = (lowerKey == "origin" ||
                                   lowerKey.find("access-control-request-") == 0);
            if (!isSafe && !isCorsProtocol) {
                bool isAllowed = std::find(allowed.begin(), allowed.end(), lowerKey) != allowed.end();
                if (!isAllowed) {
                    result.allowed = false;
                    result.blockReason = "Header '" + key + "' não permitido pelo preflight";
                    return result;
                }
            }
        }
    }

    // 4. Max-Age
    std::string maxAgeStr = getHeaderCI(preflightResponseHeaders, "Access-Control-Max-Age");
    if (!maxAgeStr.empty()) {
        try {
            result.maxAge = std::stol(maxAgeStr);
        } catch (...) {
            result.maxAge = 0;
        }
    }

    result.allowed = true;
    return result;
}

} // namespace browser_network
