#pragma once

#include "HttpEntities.h"
#include "RedirectTypes.h"
#include "security/Origin.h"
#include <atomic>
#include <memory>
#include <string>
#include <optional>

namespace browser_network {

enum class CredentialsMode {
    Omit,
    SameOrigin,
    Include
};

/**
 * @brief Representa uma requisição HTTP.
 *
 * Encapsula todos os parâmetros de uma requisição: URL, método, headers,
 * body, timeout, e configurações de redirecionamento.
 */
class HttpRequest {
public:
    HttpRequest(std::string url, HttpMethod method)
        : url_(std::move(url)), method_(method) {}

    const std::string& getUrl() const { return url_; }
    HttpMethod getMethod() const { return method_; }
    
    const HeaderMap& getHeaders() const { return headers_; }
    void addHeader(const std::string& key, const std::string& value) {
        headers_[key] = value;
    }
    void removeHeader(const std::string& key) {
        headers_.erase(key);
    }

    const ByteBuffer& getBody() const { return body_; }
    void setBody(ByteBuffer body) {
        body_ = std::move(body);
    }
    void setBody(const std::string& body) {
        body_.assign(body.begin(), body.end());
    }

    /**
     * @brief Obtém o tempo limite da requisição em milissegundos.
     */
    long getTimeout() const { return timeoutMs_; }

    /**
     * @brief Define o tempo limite da requisição.
     * @param timeoutMs O tempo limite em milissegundos.
     */
    void setTimeout(long timeoutMs) { timeoutMs_ = timeoutMs; }

    // --- Configuração de Redirecionamento ---

    /**
     * @brief Obtém o modo de redirecionamento (Fetch API).
     * @return RedirectMode atual (Follow, Error, ou Manual).
     */
    RedirectMode getRedirectMode() const { return redirectMode_; }

    /**
     * @brief Define o modo de redirecionamento.
     * @param mode RedirectMode::Follow, Error, ou Manual.
     */
    void setRedirectMode(RedirectMode mode) { redirectMode_ = mode; }

    /**
     * @brief Obtém o número máximo de redirecionamentos permitidos.
     */
    int getMaxRedirects() const { return maxRedirects_; }

    /**
     * @brief Define o número máximo de redirecionamentos.
     * @param max Limite de hops (padrão: 20, como nos navegadores).
     */
    void setMaxRedirects(int max) { maxRedirects_ = max; }

    /**
     * @brief Compatibilidade retroativa: mapeia bool para RedirectMode.
     * @param follow true → Follow, false → Manual.
     * @deprecated Usar setRedirectMode() para controle granular.
     */
    void setFollowRedirects(bool follow) {
        redirectMode_ = follow ? RedirectMode::Follow : RedirectMode::Manual;
    }

    /**
     * @brief Compatibilidade retroativa: retorna true se modo é Follow.
     * @deprecated Usar getRedirectMode() para controle granular.
     */
    bool getFollowRedirects() const {
        return redirectMode_ == RedirectMode::Follow;
    }

    CredentialsMode getCredentialsMode() const { return credentialsMode_; }
    void setCredentialsMode(CredentialsMode mode) { credentialsMode_ = mode; }

    bool usesBrowserCookieJar() const { return browserCookieJarEnabled_; }
    void setUseBrowserCookieJar(bool enabled) { browserCookieJarEnabled_ = enabled; }

    const std::optional<Origin>& getSiteForCookies() const { return siteForCookies_; }
    void setSiteForCookies(Origin origin) { siteForCookies_ = std::move(origin); }

    bool isTopLevelNavigation() const { return topLevelNavigation_; }
    void setTopLevelNavigation(bool value) { topLevelNavigation_ = value; }

    std::shared_ptr<std::atomic<bool>> getAbortSignal() const { return abortSignal_; }
    void setAbortSignal(std::shared_ptr<std::atomic<bool>> signal) {
        abortSignal_ = std::move(signal);
    }
    bool isAborted() const {
        return abortSignal_ && abortSignal_->load();
    }

private:
    std::string url_;
    HttpMethod method_;
    HeaderMap headers_;
    ByteBuffer body_;
    long timeoutMs_ = 30000;                      ///< Tempo limite padrão de 30 segundos
    RedirectMode redirectMode_ = RedirectMode::Follow; ///< Modo de redirecionamento padrão
    int maxRedirects_ = 20;                        ///< Limite de hops (padrão navegador)
    CredentialsMode credentialsMode_ = CredentialsMode::Include;
    bool browserCookieJarEnabled_ = false;
    std::optional<Origin> siteForCookies_;
    bool topLevelNavigation_ = false;
    std::shared_ptr<std::atomic<bool>> abortSignal_;
};

} // namespace browser_network
