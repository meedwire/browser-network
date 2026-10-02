#pragma once

#include "HttpEntities.h"
#include "streams/ReadableStream.h"
#include <memory>
#include <string>
#include <vector>

namespace browser_network {

/**
 * @brief Resposta HTTP com body em streaming.
 *
 * Diferente de HttpResponse que armazena todo o body em memória,
 * StreamingResponse fornece os headers imediatamente e o body
 * como um ReadableStream<ByteBuffer>, permitindo consumo
 * incremental dos dados conforme chegam da rede.
 *
 * Equivale ao comportamento de `Response` dos navegadores onde
 * `response.body` retorna um ReadableStream.
 */
class StreamingResponse {
public:
    StreamingResponse(
        int statusCode,
        std::string statusMessage,
        HeaderMap headers,
        std::shared_ptr<ReadableStream<ByteBuffer>> bodyStream
    )
        : status_{statusCode, std::move(statusMessage)}
        , headers_(std::move(headers))
        , bodyStream_(std::move(bodyStream))
        , url_("")
        , redirected_(false) {}

    /**
     * @brief Retorna o status HTTP da resposta.
     */
    const HttpStatus& getStatus() const { return status_; }

    /**
     * @brief Retorna os headers da resposta.
     */
    const HeaderMap& getHeaders() const { return headers_; }
    void setHeaders(HeaderMap headers) { headers_ = std::move(headers); }

    const std::vector<std::string>& getSetCookieHeaders() const { return setCookieHeaders_; }
    void setSetCookieHeaders(std::vector<std::string> headers) {
        setCookieHeaders_ = std::move(headers);
    }
    void addSetCookieHeader(std::string header) {
        setCookieHeaders_.push_back(std::move(header));
    }
    void clearSetCookieHeaders() { setCookieHeaders_.clear(); }

    /**
     * @brief Retorna o ReadableStream do body.
     *
     * Permite leitura incremental dos dados. Cada chamada a
     * `read()` retorna o próximo chunk disponível.
     */
    std::shared_ptr<ReadableStream<ByteBuffer>> getBody() const { return bodyStream_; }

    /**
     * @brief Retorna a URL final da resposta (após redirecionamentos).
     */
    const std::string& getUrl() const { return url_; }
    void setUrl(const std::string& url) { url_ = url; }

    /**
     * @brief Retorna se a resposta foi resultado de um redirecionamento.
     */
    bool redirected() const { return redirected_; }
    void setRedirected(bool redirected) { redirected_ = redirected; }

private:
    HttpStatus status_;
    HeaderMap headers_;
    std::vector<std::string> setCookieHeaders_;
    std::shared_ptr<ReadableStream<ByteBuffer>> bodyStream_;
    std::string url_;
    bool redirected_;
};

} // namespace browser_network
