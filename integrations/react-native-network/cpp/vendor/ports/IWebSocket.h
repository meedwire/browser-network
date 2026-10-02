#pragma once

#include "../core/WebSocketTypes.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace browser_network {

/**
 * @brief Interface abstrata para WebSocket conforme a especificação W3C.
 *
 * Define o contrato para implementações de WebSocket (IXWebSocket, plataformas nativas, etc.).
 * Segue o princípio de Inversão de Dependência (DIP) do SOLID — o código de negócio
 * depende desta abstração, não da implementação concreta.
 *
 * A API espelha a interface `WebSocket` dos navegadores:
 * - Callbacks para eventos (onOpen, onMessage, onError, onClose)
 * - Métodos para envio e controle (send, close)
 * - Propriedades de estado (getState, getUrl, getProtocol)
 */
class IWebSocket {
public:
    // Tipos de callback seguindo os eventos W3C
    using OnOpenCallback = std::function<void()>;
    using OnMessageCallback = std::function<void(const WebSocketMessage&)>;
    using OnErrorCallback = std::function<void(const WebSocketError&)>;
    using OnCloseCallback = std::function<void(const WebSocketCloseInfo&)>;

    virtual ~IWebSocket() = default;

    // --- Registro de Callbacks ---

    /**
     * @brief Registra callback para o evento `open`.
     * Disparado quando a conexão WebSocket é estabelecida com sucesso.
     */
    virtual void setOnOpen(OnOpenCallback callback) = 0;

    /**
     * @brief Registra callback para o evento `message`.
     * Disparado quando uma mensagem (texto ou binária) é recebida.
     */
    virtual void setOnMessage(OnMessageCallback callback) = 0;

    /**
     * @brief Registra callback para o evento `error`.
     * Disparado quando ocorre um erro na conexão.
     */
    virtual void setOnError(OnErrorCallback callback) = 0;

    /**
     * @brief Registra callback para o evento `close`.
     * Disparado quando a conexão é fechada (limpa ou abruptamente).
     */
    virtual void setOnClose(OnCloseCallback callback) = 0;

    // --- Controle de Conexão ---

    /**
     * @brief Inicia a conexão WebSocket.
     *
     * A conexão é assíncrona. Quando estabelecida, o callback `onOpen` é invocado.
     */
    virtual void connect() = 0;

    /**
     * @brief Envia uma mensagem de texto pelo WebSocket.
     * @param text Dados em formato string UTF-8.
     */
    virtual void send(const std::string& text) = 0;

    /**
     * @brief Envia uma mensagem binária pelo WebSocket.
     * @param data Dados em formato ByteBuffer.
     */
    virtual void send(const ByteBuffer& data) = 0;

    /**
     * @brief Inicia o fechamento da conexão WebSocket.
     * @param code Código de fechamento (padrão: 1000 = Normal Closure).
     * @param reason Motivo do fechamento (string descritiva, opcional).
     */
    virtual void close(int code = 1000, const std::string& reason = "") = 0;

    // --- Propriedades ---

    /**
     * @brief Retorna o estado atual da conexão.
     */
    virtual WebSocketState getState() const = 0;

    /**
     * @brief Retorna a URL do WebSocket.
     */
    virtual const std::string& getUrl() const = 0;

    /**
     * @brief Retorna o sub-protocolo negociado (se houver).
     */
    virtual const std::string& getProtocol() const = 0;
};

using IWebSocketPtr = std::shared_ptr<IWebSocket>;

} // namespace browser_network
