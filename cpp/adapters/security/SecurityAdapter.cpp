#include "SecurityAdapter.h"
#include "../cookies/AtomicFileCookiePersistence.h"
#include <future>

namespace browser_network {

namespace {

static bool requestUsesCredentials(const HttpRequest& request) {
    return request.getCredentialsMode() == CredentialsMode::Include;
}

static void applyCredentialsPolicy(HttpRequest& request, bool isCrossOrigin) {
    if (request.getCredentialsMode() == CredentialsMode::Omit) {
        request.removeHeader("Cookie");
        request.removeHeader("cookie");
        return;
    }

    if (request.getCredentialsMode() == CredentialsMode::SameOrigin && isCrossOrigin) {
        request.setCredentialsMode(CredentialsMode::Omit);
        request.removeHeader("Cookie");
        request.removeHeader("cookie");
    }
}

static void hideSetCookieHeaders(HeaderMap& headers) {
    headers.erase("Set-Cookie");
    headers.erase("set-cookie");
}

std::vector<std::string> extractSetCookieHeaders(const HttpResponse& response) {
    std::vector<std::string> headers = response.getSetCookieHeaders();
    if (!headers.empty()) {
        return headers;
    }

    for (const auto& key : {"Set-Cookie", "set-cookie"}) {
        auto it = response.getHeaders().find(key);
        if (it != response.getHeaders().end()) {
            headers.push_back(it->second);
        }
    }
    return headers;
}

std::vector<std::string> extractSetCookieHeaders(const StreamingResponse& response) {
    std::vector<std::string> headers = response.getSetCookieHeaders();
    if (!headers.empty()) {
        return headers;
    }

    for (const auto& key : {"Set-Cookie", "set-cookie"}) {
        auto it = response.getHeaders().find(key);
        if (it != response.getHeaders().end()) {
            headers.push_back(it->second);
        }
    }
    return headers;
}

} // namespace

SecurityAdapter::SecurityAdapter(
    std::shared_ptr<INetworkAdapter> inner,
    NetworkConfig config,
    std::shared_ptr<CookieJar> cookieJar
) : inner_(std::move(inner)),
    config_(std::move(config)),
    cookieJar_(std::move(cookieJar)) {

    /**
     * Resolver a origin efetiva uma vez na construção.
     * Isso garante determinismo: a origin não muda durante a vida do adapter.
     */
    if (config_.mode == OriginMode::BrowserEmulation) {
        effectiveOrigin_ = SecurityPolicy::resolveOrigin(config_);
        if (!cookieJar_) {
            std::shared_ptr<ICookiePersistence> persistence;
            if (!config_.cookiePersistencePath.empty()) {
                persistence = std::make_shared<AtomicFileCookiePersistence>(
                    config_.cookiePersistencePath);
            }
            cookieJar_ = std::make_shared<CookieJar>(std::move(persistence));
        }
    }
}

std::future<HttpResponse> SecurityAdapter::send(const HttpRequest& request) {
    /**
     * Modo Native: pass-through direto, sem overhead.
     */
    if (config_.mode == OriginMode::Native) {
        return inner_->send(request);
    }

    /**
     * Modo BrowserEmulation: aplicar regras de segurança completas.
     */
    return std::async(std::launch::async, [this, request]() -> HttpResponse {
        // 1. Clonar request para adicionar headers de segurança
        HttpRequest secureReq = request;

        // 2. Emitir header Origin
        secureReq.addHeader("Origin", effectiveOrigin_.serialize());
        secureReq.setUseBrowserCookieJar(true);
        secureReq.setSiteForCookies(effectiveOrigin_);
        secureReq.setTopLevelNavigation(false);

        // 3. Verificar se é cross-origin
        auto requestOrigin = Origin::fromUrl(request.getUrl());
        bool isCrossOrigin = requestOrigin.has_value() &&
                             !SecurityPolicy::isSameOrigin(effectiveOrigin_, requestOrigin.value());
        applyCredentialsPolicy(secureReq, isCrossOrigin);

        // 4. Se preflight necessário (cross-origin + request complexo)
        if (isCrossOrigin && CorsPolicy::isPreflightRequired(request)) {
            performPreflight(secureReq);
        }

        // 5. Enviar request real
        auto futureRes = inner_->send(secureReq);
        HttpResponse response = futureRes.get();
        if (response.getUrl().empty()) {
            response.setUrl(request.getUrl());
        }

        // 6. Validar CORS (apenas cross-origin)
        if (isCrossOrigin) {
            validateCors(response, requestUsesCredentials(secureReq));
        }

        // 7. Armazenar + ocultar cookies
        enforceCookieSecurity(response);

        return response;
    });
}

std::future<StreamingResponse> SecurityAdapter::sendStreaming(const HttpRequest& request) {
    /**
     * Modo Native: pass-through direto.
     */
    if (config_.mode == OriginMode::Native) {
        return inner_->sendStreaming(request);
    }

    /**
     * Modo BrowserEmulation: adicionar Origin header.
     * CORS para streaming é validado nos headers iniciais.
     */
    return std::async(std::launch::async, [this, request]() -> StreamingResponse {
        HttpRequest secureReq = request;
        secureReq.addHeader("Origin", effectiveOrigin_.serialize());
        secureReq.setUseBrowserCookieJar(true);
        secureReq.setSiteForCookies(effectiveOrigin_);
        secureReq.setTopLevelNavigation(false);

        auto requestOrigin = Origin::fromUrl(request.getUrl());
        bool isCrossOrigin = requestOrigin.has_value() &&
                             !SecurityPolicy::isSameOrigin(effectiveOrigin_, requestOrigin.value());
        applyCredentialsPolicy(secureReq, isCrossOrigin);

        if (isCrossOrigin && CorsPolicy::isPreflightRequired(request)) {
            performPreflight(secureReq);
        }

        auto futureRes = inner_->sendStreaming(secureReq);
        StreamingResponse response = futureRes.get();
        if (response.getUrl().empty()) {
            response.setUrl(request.getUrl());
        }

        if (isCrossOrigin) {
            HttpResponse responseHeadersOnly(response.getStatus().code, "");
            responseHeadersOnly.setHeaders(response.getHeaders());
            responseHeadersOnly.setUrl(response.getUrl());
            validateCors(responseHeadersOnly, requestUsesCredentials(secureReq));
        }

        enforceCookieSecurity(response);

        return response;
    });
}

void SecurityAdapter::performPreflight(const HttpRequest& request) {
    std::string methodStr = CorsPolicy::methodToString(request.getMethod());

    // Verificar cache de preflight
    if (preflightCache_.hasValidEntry(effectiveOrigin_, request.getUrl(), methodStr)) {
        return; // Preflight cacheado, pular
    }

    /**
     * Construir request OPTIONS para preflight.
     *
     * Headers obrigatórios (Fetch Standard §3.2.3):
     *   - Origin
     *   - Access-Control-Request-Method
     *   - Access-Control-Request-Headers (se houver headers não-simples)
     */
    HttpRequest preflight(request.getUrl(), HttpMethod::OPTIONS);
    preflight.addHeader("Origin", effectiveOrigin_.serialize());
    preflight.addHeader("Access-Control-Request-Method", methodStr);

    // Coletar headers não-simples
    const auto& safe = CorsPolicy::safelistedHeaders();
    std::string customHeaders;
    for (const auto& [key, value] : request.getHeaders()) {
        std::string lowerKey = key;
        std::transform(lowerKey.begin(), lowerKey.end(), lowerKey.begin(), ::tolower);
        bool isSafe = std::find(safe.begin(), safe.end(), lowerKey) != safe.end();
        if (!isSafe && lowerKey != "origin") {
            if (!customHeaders.empty()) customHeaders += ", ";
            customHeaders += key;
        }
    }
    if (!customHeaders.empty()) {
        preflight.addHeader("Access-Control-Request-Headers", customHeaders);
    }

    // Enviar preflight
    auto futureRes = inner_->send(preflight);
    HttpResponse preflightRes = futureRes.get();

    // Validar resposta do preflight
    auto result = CorsPolicy::validatePreflight(
        effectiveOrigin_,
        request.getMethod(),
        request.getHeaders(),
        preflightRes.getHeaders()
    );

    if (!result.allowed) {
        throw CorsBlockedError(result.blockReason);
    }

    // Cachear resultado se Max-Age > 0
    if (result.maxAge > 0) {
        std::string allowMethods = "";
        std::string allowHeaders = "";
        auto headers = preflightRes.getHeaders();
        for (const auto& [k, v] : headers) {
            std::string lk = k;
            std::transform(lk.begin(), lk.end(), lk.begin(), ::tolower);
            if (lk == "access-control-allow-methods") allowMethods = v;
            if (lk == "access-control-allow-headers") allowHeaders = v;
        }
        preflightCache_.store(
            effectiveOrigin_, request.getUrl(), methodStr,
            allowMethods, allowHeaders, result.maxAge
        );
    }
}

void SecurityAdapter::validateCors(const HttpResponse& response, bool withCredentials) {
    auto result = CorsPolicy::validateResponse(
        effectiveOrigin_,
        response.getHeaders(),
        withCredentials
    );

    if (!result.allowed) {
        throw CorsBlockedError(result.blockReason);
    }
}

void SecurityAdapter::enforceCookieSecurity(HttpResponse& response) {
    if (cookieJar_) {
        auto headers = extractSetCookieHeaders(response);
        cookieJar_->storeFromResponse(
            response.getUrl().empty() ? response.getRedirectInfo().finalUrl : response.getUrl(),
            headers);
    }

    HeaderMap headers = response.getHeaders();
    hideSetCookieHeaders(headers);
    response.setHeaders(std::move(headers));
    response.clearSetCookieHeaders();
}

void SecurityAdapter::enforceCookieSecurity(StreamingResponse& response) {
    if (cookieJar_) {
        auto headers = extractSetCookieHeaders(response);
        cookieJar_->storeFromResponse(
            response.getUrl(),
            headers);
    }

    HeaderMap headers = response.getHeaders();
    hideSetCookieHeaders(headers);
    response.setHeaders(std::move(headers));
    response.clearSetCookieHeaders();
}

} // namespace browser_network
