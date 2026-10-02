#pragma once

#include <deque>
#include <mutex>
#include <condition_variable>
#include <optional>
#include <functional>
#include <atomic>
#include <stdexcept>

namespace browser_network {

/**
 * @brief Estado do stream.
 *
 * Representa o ciclo de vida de um ReadableStream conforme
 * a especificação WHATWG Streams.
 */
enum class StreamState {
    Readable,  ///< Dados podem ser lidos ou enfileirados
    Closed,    ///< Produtor finalizou; consumidor pode drenar a fila
    Errored    ///< Ocorreu um erro; nenhuma leitura adicional é possível
};

/**
 * @brief Implementação de ReadableStream seguindo a especificação WHATWG.
 *
 * Permite que um produtor enfileire chunks de dados e um consumidor
 * os leia de forma incremental. Suporta backpressure via high-water mark
 * e é thread-safe para uso entre threads de produção e consumo.
 *
 * @tparam T Tipo dos chunks de dados (normalmente ByteBuffer).
 */
template <typename T>
class ReadableStream {
public:
    /**
     * @brief Constrói um ReadableStream com um limite de backpressure.
     * @param highWaterMark Número máximo de chunks na fila antes de
     *        bloquear o produtor. Valor 0 significa sem limite.
     */
    explicit ReadableStream(size_t highWaterMark = 0)
        : highWaterMark_(highWaterMark), state_(StreamState::Readable) {}

    ~ReadableStream() = default;

    // Não copiável, não movível
    ReadableStream(const ReadableStream&) = delete;
    ReadableStream& operator=(const ReadableStream&) = delete;

    // --- API do Produtor ---

    /**
     * @brief Enfileira um chunk de dados no stream.
     *
     * Se o high-water mark foi atingido, bloqueia o produtor até que
     * o consumidor leia dados suficientes. Lança exceção se o stream
     * já foi fechado ou está em estado de erro.
     *
     * @param chunk Dados a serem enfileirados.
     */
    void enqueue(T chunk) {
        std::unique_lock<std::mutex> lock(mutex_);

        if (state_ != StreamState::Readable) {
            throw std::runtime_error("Não é possível enfileirar: stream não está no estado Readable.");
        }

        // Backpressure: aguardar se a fila atingiu o limite
        if (highWaterMark_ > 0) {
            notFull_.wait(lock, [this] {
                return queue_.size() < highWaterMark_ || state_ != StreamState::Readable;
            });
        }

        if (state_ != StreamState::Readable) {
            return; // Stream foi cancelado enquanto aguardava
        }

        queue_.push_back(std::move(chunk));
        notEmpty_.notify_one();
    }

    /**
     * @brief Fecha o stream, indicando que não haverá mais dados.
     *
     * O consumidor ainda pode ler os dados restantes na fila.
     */
    void close() {
        std::lock_guard<std::mutex> lock(mutex_);
        state_ = StreamState::Closed;
        notEmpty_.notify_all();
        notFull_.notify_all();
    }

    /**
     * @brief Coloca o stream em estado de erro.
     * @param errorMsg Mensagem descritiva do erro.
     */
    void error(const std::string& errorMsg) {
        std::lock_guard<std::mutex> lock(mutex_);
        state_ = StreamState::Errored;
        errorMessage_ = errorMsg;
        notEmpty_.notify_all();
        notFull_.notify_all();
    }

    // --- API do Consumidor ---

    /**
     * @brief Lê o próximo chunk do stream.
     *
     * Bloqueia até que um chunk esteja disponível. Retorna std::nullopt
     * quando o stream foi fechado e a fila está vazia (fim do stream).
     * Lança exceção se o stream está em estado de erro.
     *
     * @return O próximo chunk, ou std::nullopt se o stream terminou.
     */
    std::optional<T> read() {
        std::unique_lock<std::mutex> lock(mutex_);

        notEmpty_.wait(lock, [this] {
            return !queue_.empty() || state_ != StreamState::Readable;
        });

        if (state_ == StreamState::Errored) {
            throw std::runtime_error("Erro no stream: " + errorMessage_);
        }

        if (queue_.empty()) {
            // Stream fechado e fila vazia = fim
            return std::nullopt;
        }

        T chunk = std::move(queue_.front());
        queue_.pop_front();
        notFull_.notify_one();
        return chunk;
    }

    /**
     * @brief Cancela o stream, sinalizando ao produtor para parar.
     */
    void cancel() {
        std::lock_guard<std::mutex> lock(mutex_);
        state_ = StreamState::Closed;
        queue_.clear();
        notEmpty_.notify_all();
        notFull_.notify_all();
    }

    // --- Consultas de Estado ---

    /**
     * @brief Verifica se o stream foi completamente consumido.
     * @return true se o stream está fechado e a fila está vazia.
     */
    bool isComplete() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return state_ == StreamState::Closed && queue_.empty();
    }

    /**
     * @brief Retorna o estado atual do stream.
     */
    StreamState getState() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return state_;
    }

    /**
     * @brief Retorna o número de chunks atualmente na fila.
     */
    size_t queueSize() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable notEmpty_;
    std::condition_variable notFull_;
    std::deque<T> queue_;
    size_t highWaterMark_;
    StreamState state_;
    std::string errorMessage_;
};

} // namespace browser_network
