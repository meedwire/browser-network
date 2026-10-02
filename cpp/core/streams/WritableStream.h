#pragma once

#include <functional>
#include <mutex>
#include <atomic>
#include <stdexcept>
#include <string>

namespace browser_network {

/**
 * @brief Estado do WritableStream.
 */
enum class WritableState {
    Writable,  ///< Aceitando dados
    Closed,    ///< Fechado normalmente
    Errored    ///< Em estado de erro
};

/**
 * @brief Implementação de WritableStream seguindo a especificação WHATWG.
 *
 * Permite que um produtor envie chunks de dados para um consumidor (sink)
 * de forma controlada. O sink é definido como um callback fornecido
 * na construção do stream.
 *
 * @tparam T Tipo dos chunks de dados.
 */
template <typename T>
class WritableStream {
public:
    /**
     * @brief Callback invocado para cada chunk escrito.
     */
    using SinkCallback = std::function<void(T)>;

    /**
     * @brief Callback invocado quando o stream é fechado.
     */
    using CloseCallback = std::function<void()>;

    /**
     * @brief Callback invocado quando o stream é abortado.
     */
    using AbortCallback = std::function<void(const std::string&)>;

    /**
     * @brief Constrói um WritableStream com callbacks de sink.
     * @param onWrite Chamado para cada chunk escrito.
     * @param onClose Chamado quando o stream é fechado (opcional).
     * @param onAbort Chamado quando o stream é abortado (opcional).
     */
    explicit WritableStream(
        SinkCallback onWrite,
        CloseCallback onClose = nullptr,
        AbortCallback onAbort = nullptr
    )
        : onWrite_(std::move(onWrite))
        , onClose_(std::move(onClose))
        , onAbort_(std::move(onAbort))
        , state_(WritableState::Writable) {}

    ~WritableStream() = default;

    WritableStream(const WritableStream&) = delete;
    WritableStream& operator=(const WritableStream&) = delete;

    /**
     * @brief Escreve um chunk no stream.
     *
     * Invoca o callback de sink com o chunk fornecido.
     * Lança exceção se o stream não está no estado Writable.
     *
     * @param chunk Dados a serem escritos.
     */
    void write(T chunk) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (state_ != WritableState::Writable) {
            throw std::runtime_error("Não é possível escrever: stream não está no estado Writable.");
        }

        if (onWrite_) {
            onWrite_(std::move(chunk));
        }
    }

    /**
     * @brief Fecha o stream normalmente.
     *
     * Sinaliza que não haverá mais dados.
     */
    void close() {
        std::lock_guard<std::mutex> lock(mutex_);

        if (state_ != WritableState::Writable) {
            return;
        }

        state_ = WritableState::Closed;
        if (onClose_) {
            onClose_();
        }
    }

    /**
     * @brief Aborta o stream com uma razão de erro.
     * @param reason Mensagem de erro descritiva.
     */
    void abort(const std::string& reason) {
        std::lock_guard<std::mutex> lock(mutex_);
        state_ = WritableState::Errored;
        if (onAbort_) {
            onAbort_(reason);
        }
    }

    /**
     * @brief Retorna o estado atual do stream.
     */
    WritableState getState() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return state_;
    }

private:
    mutable std::mutex mutex_;
    SinkCallback onWrite_;
    CloseCallback onClose_;
    AbortCallback onAbort_;
    WritableState state_;
};

} // namespace browser_network
