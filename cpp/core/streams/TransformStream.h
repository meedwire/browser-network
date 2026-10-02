#pragma once

#include "ReadableStream.h"
#include "WritableStream.h"
#include <functional>
#include <memory>
#include <thread>

namespace browser_network {

/**
 * @brief Implementação de TransformStream seguindo a especificação WHATWG.
 *
 * Conecta um WritableStream (lado de entrada) a um ReadableStream (lado de saída),
 * aplicando uma função de transformação em cada chunk. Útil para pipelines de
 * processamento de dados como decompressão, parsing, etc.
 *
 * @tparam In Tipo dos chunks de entrada.
 * @tparam Out Tipo dos chunks de saída.
 */
template <typename In, typename Out>
class TransformStream {
public:
    /**
     * @brief Função de transformação aplicada a cada chunk.
     */
    using TransformFunction = std::function<Out(In)>;

    /**
     * @brief Constrói um TransformStream com uma função de transformação.
     * @param transform Função que converte In -> Out.
     * @param highWaterMark Limite de backpressure do ReadableStream interno.
     */
    explicit TransformStream(TransformFunction transform, size_t highWaterMark = 0)
        : transform_(std::move(transform))
        , readable_(std::make_shared<ReadableStream<Out>>(highWaterMark))
    {
        // O WritableStream recebe chunks In e os transforma em Out
        writable_ = std::make_shared<WritableStream<In>>(
            // onWrite: aplica transformação e enfileira no readable
            [this](In chunk) {
                Out transformed = transform_(std::move(chunk));
                readable_->enqueue(std::move(transformed));
            },
            // onClose: fecha o readable side
            [this]() {
                readable_->close();
            },
            // onAbort: propaga erro para o readable
            [this](const std::string& reason) {
                readable_->error(reason);
            }
        );
    }

    ~TransformStream() = default;

    TransformStream(const TransformStream&) = delete;
    TransformStream& operator=(const TransformStream&) = delete;

    /**
     * @brief Retorna o lado de escrita (writable side) do transform.
     *
     * Use-o para escrever dados de entrada que serão transformados.
     */
    std::shared_ptr<WritableStream<In>> getWritable() const { return writable_; }

    /**
     * @brief Retorna o lado de leitura (readable side) do transform.
     *
     * Use-o para ler os dados transformados.
     */
    std::shared_ptr<ReadableStream<Out>> getReadable() const { return readable_; }

private:
    TransformFunction transform_;
    std::shared_ptr<ReadableStream<Out>> readable_;
    std::shared_ptr<WritableStream<In>> writable_;
};

} // namespace browser_network
