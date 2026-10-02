#pragma once

#include "HttpEntities.h"
#include "RedirectTypes.h"
#include <string>
#include <vector>

namespace browser_network {

/**
 * @brief Representa a resposta de uma requisição HTTP.
 *
 * Contém o status, headers, body, e informações de redirecionamento.
 * As propriedades `url` e `redirectInfo` espelham `Response.url` e
 * `Response.redirected` da Fetch API dos navegadores.
 */
class HttpResponse {
public:
    HttpResponse(int statusCode, std::string statusMessage)
        : status_{statusCode, std::move(statusMessage)} {}

    const HttpStatus& getStatus() const { return status_; }
    
    const HeaderMap& getHeaders() const { return headers_; }
    void setHeaders(HeaderMap headers) {
        headers_ = std::move(headers);
    }

    const std::vector<std::string>& getSetCookieHeaders() const { return setCookieHeaders_; }
    void setSetCookieHeaders(std::vector<std::string> headers) {
        setCookieHeaders_ = std::move(headers);
    }
    void addSetCookieHeader(std::string header) {
        setCookieHeaders_.push_back(std::move(header));
    }
    void clearSetCookieHeaders() { setCookieHeaders_.clear(); }

    const ByteBuffer& getBody() const { return body_; }
    void setBody(ByteBuffer body) {
        body_ = std::move(body);
    }

    /**
     * @brief Retorna a URL final após redirecionamentos.
     *
     * Equivale a `Response.url` nos navegadores. Se não houve redirect,
     * é a URL original da requisição.
     */
    const std::string& getUrl() const { return url_; }
    void setUrl(std::string url) { url_ = std::move(url); }

    /**
     * @brief Retorna informações sobre redirecionamentos.
     *
     * Contém a flag `redirected`, a URL final, e a cadeia completa de hops.
     */
    const RedirectInfo& getRedirectInfo() const { return redirectInfo_; }
    void setRedirectInfo(RedirectInfo info) { redirectInfo_ = std::move(info); }

private:
    HttpStatus status_;
    HeaderMap headers_;
    std::vector<std::string> setCookieHeaders_;
    ByteBuffer body_;
    std::string url_;
    RedirectInfo redirectInfo_;
};

} // namespace browser_network
