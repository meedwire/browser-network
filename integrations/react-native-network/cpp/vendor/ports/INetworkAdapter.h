#pragma once

#include "../core/HttpRequest.h"
#include "../core/HttpResponse.h"
#include "../core/StreamingResponse.h"
#include <future>
#include <memory>

namespace browser_network {

/**
 * @brief Interface do adaptador de rede.
 *
 * Define o contrato para implementações de rede (Curl, plataformas nativas, etc.).
 * Segue o princípio de Interface Segregation (ISP) do SOLID.
 */
class INetworkAdapter {
public:
    virtual ~INetworkAdapter() = default;

    /**
     * @brief Envia uma requisição e retorna a resposta completa.
     * @param request A requisição HTTP a ser enviada.
     * @return Future que resolve para HttpResponse com o body completo em memória.
     */
    virtual std::future<HttpResponse> send(const HttpRequest& request) = 0;

    /**
     * @brief Envia uma requisição e retorna uma resposta em streaming.
     *
     * Os headers são disponibilizados imediatamente e o body é retornado
     * como um ReadableStream<ByteBuffer>, permitindo consumo incremental
     * dos dados conforme chegam da rede. Equivale ao `response.body`
     * dos navegadores (WHATWG Streams API).
     *
     * @param request A requisição HTTP a ser enviada.
     * @return Future que resolve para StreamingResponse com body em stream.
     */
    virtual std::future<StreamingResponse> sendStreaming(const HttpRequest& request) = 0;
};

using INetworkAdapterPtr = std::shared_ptr<INetworkAdapter>;

} // namespace browser_network

