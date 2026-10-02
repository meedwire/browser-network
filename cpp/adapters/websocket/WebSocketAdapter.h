#pragma once

#include "../../core/security/Origin.h"
#include "../../ports/IWebSocket.h"
#include <ixwebsocket/IXWebSocket.h>
#include <ixwebsocket/IXWebSocketHttpHeaders.h>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace browser_network {

class CookieJar;

/**
 * @brief Adaptador WebSocket baseado em IXWebSocket.
 *
 * Implementa IWebSocket usando a biblioteca IXWebSocket para
 * comunicação bidirecional via ws:// e wss://. Mapeia os eventos
 * internos do IXWebSocket para os callbacks da interface W3C.
 *
 * Suporta:
 * - Mensagens de texto e binárias
 * - Auto-ping/pong (transparente, como no navegador)
 * - TLS/SSL para conexões seguras (wss://)
 * - Códigos de fechamento conforme RFC 6455
 */
class WebSocketAdapter : public IWebSocket {
public:
    /**
     * @brief Constrói o adaptador com a URL do servidor WebSocket.
     * @param url URL do servidor (ws:// ou wss://).
     * @param protocols Sub-protocolos desejados (opcional).
     */
    explicit WebSocketAdapter(const std::string& url,
                              const std::vector<std::string>& protocols = {},
                              std::shared_ptr<CookieJar> cookieJar = nullptr,
                              std::optional<Origin> siteForCookies = std::nullopt);
    ~WebSocketAdapter() override;

    WebSocketAdapter(const WebSocketAdapter&) = delete;
    WebSocketAdapter& operator=(const WebSocketAdapter&) = delete;

    // --- Registro de Callbacks ---
    void setOnOpen(OnOpenCallback callback) override;
    void setOnMessage(OnMessageCallback callback) override;
    void setOnError(OnErrorCallback callback) override;
    void setOnClose(OnCloseCallback callback) override;

    // --- Controle ---
    void connect() override;
    void send(const std::string& text) override;
    void send(const ByteBuffer& data) override;
    void close(int code = 1000, const std::string& reason = "") override;

    // --- Propriedades ---
    WebSocketState getState() const override;
    const std::string& getUrl() const override;
    const std::string& getProtocol() const override;

private:
    ix::WebSocket ws_;
    std::string url_;
    std::string protocol_;
    ix::WebSocketHttpHeaders baseHeaders_;
    std::shared_ptr<CookieJar> cookieJar_;
    std::optional<Origin> siteForCookies_;
    mutable std::mutex callbackMutex_;

    OnOpenCallback onOpen_;
    OnMessageCallback onMessage_;
    OnErrorCallback onError_;
    OnCloseCallback onClose_;

    /**
     * @brief Configura os handlers internos do IXWebSocket.
     *
     * Mapeia eventos do IXWebSocket para os callbacks registrados na interface.
     */
    void setupEventHandlers();

    /**
     * @brief Aplica headers dinâmicos do handshake antes de iniciar a conexão.
     */
    void applyHandshakeHeaders();

    /**
     * @brief Armazena cookies recebidos no handshake HTTP 101.
     */
    void storeHandshakeCookies(const ix::WebSocketHttpHeaders& headers);
};

} // namespace browser_network
