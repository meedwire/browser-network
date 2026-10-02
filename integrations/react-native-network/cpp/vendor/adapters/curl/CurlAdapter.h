#pragma once

#include "../../ports/INetworkAdapter.h"
#include "../../core/security/CookieJar.h"
#include <curl/curl.h>
#include <future>
#include <string>

namespace browser_network {

/**
 * @brief Adaptador de rede baseado em libcurl.
 *
 * Implementa INetworkAdapter para comunicação HTTP/HTTPS.
 * Suporta requisições tradicionais (body completo) e streaming
 * (body via ReadableStream). Mantém sessão de cookies via CURLSH.
 */
class CurlAdapter : public INetworkAdapter {
public:
    explicit CurlAdapter(std::shared_ptr<CookieJar> cookieJar = nullptr);
    ~CurlAdapter() override;

    CurlAdapter(const CurlAdapter&) = delete;
    CurlAdapter& operator=(const CurlAdapter&) = delete;

    /**
     * @brief Envia uma requisição e retorna a resposta com body completo.
     */
    std::future<HttpResponse> send(const HttpRequest& request) override;

    /**
     * @brief Envia uma requisição e retorna o body como ReadableStream.
     *
     * Os headers e o status são disponibilizados assim que chegam.
     * O body é alimentado incrementalmente no stream conforme os dados
     * são recebidos da rede.
     */
    std::future<StreamingResponse> sendStreaming(const HttpRequest& request) override;

    void setCookieJar(std::shared_ptr<CookieJar> cookieJar) { cookieJar_ = std::move(cookieJar); }

private:
    CURLSH* share_;
    std::shared_ptr<CookieJar> cookieJar_;

    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp);
    static size_t HeaderCallback(char* buffer, size_t size, size_t nitems, void* userdata);
    static void LockCallback(CURL* handle, curl_lock_data data, curl_lock_access access, void* userptr);
    static void UnlockCallback(CURL* handle, curl_lock_data data, void* userptr);

    /**
     * @brief Callback de escrita para modo streaming.
     *
     * Enfileira cada chunk recebido no ReadableStream associado.
     */
    static size_t StreamWriteCallback(void* contents, size_t size, size_t nmemb, void* userp);

    HttpResponse performRequest(const HttpRequest& request);
    void performStreamingRequest(const HttpRequest& request, std::shared_ptr<ReadableStream<ByteBuffer>> bodyStream, std::promise<StreamingResponse> promise);
};

} // namespace browser_network
