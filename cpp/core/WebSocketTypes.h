#pragma once

#include "HttpEntities.h"
#include <string>
#include <variant>

namespace browser_network {

/**
 * @brief Estado do WebSocket conforme a especificação W3C.
 *
 * Espelha os valores de `WebSocket.readyState` dos navegadores:
 * - CONNECTING (0): Conexão ainda não estabelecida.
 * - OPEN (1): Conexão estabelecida, comunicação bidirecional possível.
 * - CLOSING (2): Processo de fechamento iniciado.
 * - CLOSED (3): Conexão fechada ou não pôde ser aberta.
 */
enum class WebSocketState {
    Connecting = 0,
    Open = 1,
    Closing = 2,
    Closed = 3
};

/**
 * @brief Mensagem recebida ou enviada via WebSocket.
 *
 * Suporta dados de texto (string UTF-8) ou binários (ByteBuffer),
 * conforme a especificação W3C que permite ambos os formatos.
 */
struct WebSocketMessage {
    std::variant<std::string, ByteBuffer> data;
    bool isBinary;

    /**
     * @brief Obtém os dados como string (para mensagens de texto).
     * @return Referência constante para a string.
     * @throws std::bad_variant_access se a mensagem é binária.
     */
    const std::string& asText() const { return std::get<std::string>(data); }

    /**
     * @brief Obtém os dados como ByteBuffer (para mensagens binárias).
     * @return Referência constante para o ByteBuffer.
     * @throws std::bad_variant_access se a mensagem é texto.
     */
    const ByteBuffer& asBinary() const { return std::get<ByteBuffer>(data); }
};

/**
 * @brief Informações do evento de fechamento do WebSocket.
 *
 * Conforme a especificação W3C, o evento `close` contém:
 * - `code`: Código numérico indicando a razão do fechamento (ex: 1000 = normal).
 * - `reason`: String descritiva do motivo do fechamento.
 * - `wasClean`: Indica se o fechamento foi limpo (handshake completo).
 */
struct WebSocketCloseInfo {
    int code = 1000;
    std::string reason;
    bool wasClean = false;
};

/**
 * @brief Informações de erro do WebSocket.
 */
struct WebSocketError {
    std::string message;
    int code = 0;
};

} // namespace browser_network
