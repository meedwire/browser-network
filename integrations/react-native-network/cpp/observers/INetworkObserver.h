#pragma once

#include <string>
#include <vector>
#include <optional>
#include <core/HttpRequest.h>
#include <core/HttpResponse.h>

namespace browser_network {

/**
 * @struct NetworkRequestEvent
 * @brief Dados da requisição para observadores.
 */
struct NetworkRequestEvent {
    std::string requestId;
    std::string url;
    std::string method;
    HeaderMap headers;
    ByteBuffer body;
};

/**
 * @struct NetworkResponseEvent
 * @brief Dados da resposta para observadores.
 */
struct NetworkResponseEvent {
    std::string requestId;
    int status;
    std::string url;
    HeaderMap headers;
    ByteBuffer body;
};

/**
 * @struct StreamingResponseStartEvent
 * @brief Dados do início de uma resposta streaming (quando headers chegam).
 */
struct StreamingResponseStartEvent {
    std::string requestId;
    int status;
    std::string url;
    HeaderMap headers;
};

/**
 * @class INetworkObserver
 * @brief Interface para observar o ciclo de vida das requisições de rede.
 */
class INetworkObserver {
public:
    virtual ~INetworkObserver() = default;

    virtual bool isEnabled() const = 0;

    virtual void onBeforeRequest(const NetworkRequestEvent& event) = 0;
    virtual void onResponseReceived(const NetworkResponseEvent& event) = 0;
    virtual void onError(const std::string& requestId, const std::string& message) = 0;

    // Métodos para streaming - implementações default para compatibilidade
    virtual void onStreamingResponseStart(const StreamingResponseStartEvent& event) {
        // Default: converter para onResponseReceived com body vazio
        NetworkResponseEvent respEvent;
        respEvent.requestId = event.requestId;
        respEvent.status = event.status;
        respEvent.url = event.url;
        respEvent.headers = event.headers;
        // body vazio
        onResponseReceived(respEvent);
    }

    virtual void onStreamingDataReceived(const std::string& requestId, size_t length) {
        // Default: noop - observers que precisam podem override
    }

    virtual void onStreamingResponseEnd(const std::string& requestId, const std::string& body, bool base64Encoded) {
        // Default: noop - para observers que precisam armazenar o body
    }
};

} // namespace browser_network
