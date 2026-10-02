#include "CurlAdapter.h"
#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <algorithm>

namespace browser_network {

namespace {

// --- Funções utilitárias para redirect ---

/**
 * @brief Extrai a origem (scheme + host + porta) de uma URL.
 *
 * Usado para detectar redirects cross-origin. Quando a origem muda,
 * headers sensíveis (Authorization, Cookie, Proxy-Authorization) devem
 * ser removidos para evitar vazamento de credenciais entre domínios.
 *
 * Exemplo: "https://api.example.com:8080/path" → "https://api.example.com:8080"
 */
static std::string extractOrigin(const std::string& url) {
    size_t schemeEnd = url.find("://");
    if (schemeEnd == std::string::npos) return "";
    size_t hostStart = schemeEnd + 3;
    size_t hostEnd = url.find('/', hostStart);
    if (hostEnd == std::string::npos) {
        return url; // URL sem path (ex: "https://example.com")
    }
    return url.substr(0, hostEnd);
}

/**
 * @brief Normaliza um path resolvendo segmentos `.` e `..`.
 *
 * Conforme RFC 3986, Section 5.2.4 — Remove Dot Segments.
 * Necessário para resolver Location headers como "../auth" ou "./login".
 */
static std::string normalizePath(const std::string& path) {
    std::vector<std::string> segments;
    std::string current;
    
    for (size_t i = 0; i < path.size(); ++i) {
        if (path[i] == '/') {
            if (current == "..") {
                if (!segments.empty()) segments.pop_back();
            } else if (current != "." && !current.empty()) {
                segments.push_back(current);
            }
            current.clear();
        } else {
            current += path[i];
        }
    }
    // Último segmento
    if (current == "..") {
        if (!segments.empty()) segments.pop_back();
    } else if (current != "." && !current.empty()) {
        segments.push_back(current);
    }
    
    std::string result = "/";
    for (size_t i = 0; i < segments.size(); ++i) {
        result += segments[i];
        if (i + 1 < segments.size()) result += "/";
    }
    // Se o path original terminava com /, manter
    if (path.size() > 1 && path.back() == '/') result += "/";
    
    return result;
}

/**
 * @brief Remove headers sensíveis que não devem cruzar origens.
 *
 * Quando um redirect muda de origem (ex: api.a.com → api.b.com),
 * o navegador remove automaticamente headers que podem vazar credenciais.
 * Esta função implementa esse comportamento de segurança.
 */
static void stripSensitiveHeaders(HeaderMap& headers) {
    headers.erase("Authorization");
    headers.erase("authorization");
    headers.erase("Cookie");
    headers.erase("cookie");
    headers.erase("Proxy-Authorization");
    headers.erase("proxy-authorization");
}

static bool shouldUseCookieStore(const HttpRequest& request) {
    return request.getCredentialsMode() != CredentialsMode::Omit &&
           !request.usesBrowserCookieJar();
}

struct ResponseHeaderAccumulator {
    HeaderMap* headers = nullptr;
    std::vector<std::string>* setCookieHeaders = nullptr;
};

CookieRequestContext buildCookieContext(
    const HttpRequest& request,
    const std::string& url,
    HttpMethod method
) {
    CookieRequestContext context;
    context.requestUrl = url;
    context.siteForCookies = request.getSiteForCookies();
    context.credentialsMode = request.getCredentialsMode();
    context.method = method;
    context.isTopLevelNavigation = request.isTopLevelNavigation();
    return context;
}

bool isSetCookieHeader(const std::string& key) {
    std::string lower = key;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return lower == "set-cookie";
}

static int ProgressCallback(void* clientp,
                            curl_off_t dltotal,
                            curl_off_t dlnow,
                            curl_off_t ultotal,
                            curl_off_t ulnow) {
    (void)dltotal;
    (void)dlnow;
    (void)ultotal;
    (void)ulnow;

    auto* abortSignal = static_cast<std::atomic<bool>*>(clientp);
    if (abortSignal != nullptr && abortSignal->load()) {
        return 1;
    }

    return 0;
}

static void ConfigureAbortHandling(CURL* curl, const HttpRequest& request) {
    auto abortSignal = request.getAbortSignal();
    if (!abortSignal) {
        return;
    }

    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, ProgressCallback);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, abortSignal.get());
}

} // namespace

// Global mutexes for CURLSH locking (Simple implementation)
// In a real app, you might want a map of mutexes based on data type, or a more sophisticated approach.
static std::mutex s_cookieMutex;
static std::mutex s_dnsMutex;
static std::mutex s_shareMutex;

CurlAdapter::CurlAdapter(std::shared_ptr<CookieJar> cookieJar)
    : cookieJar_(std::move(cookieJar)) {
    curl_global_init(CURL_GLOBAL_ALL);
    share_ = curl_share_init();
    curl_share_setopt(share_, CURLSHOPT_SHARE, CURL_LOCK_DATA_COOKIE);
    // curl_share_setopt(share_, CURLSHOPT_SHARE, CURL_LOCK_DATA_DNS); // Optional: Share DNS cache too
    
    curl_share_setopt(share_, CURLSHOPT_LOCKFUNC, LockCallback);
    curl_share_setopt(share_, CURLSHOPT_UNLOCKFUNC, UnlockCallback);
    curl_share_setopt(share_, CURLSHOPT_USERDATA, nullptr);
}

CurlAdapter::~CurlAdapter() {
    curl_share_cleanup(share_);
    curl_global_cleanup();
}

void CurlAdapter::LockCallback(CURL* handle, curl_lock_data data, curl_lock_access access, void* userptr) {
    (void)handle; (void)access; (void)userptr;
    switch (data) {
        case CURL_LOCK_DATA_COOKIE: s_cookieMutex.lock(); break;
        case CURL_LOCK_DATA_DNS:    s_dnsMutex.lock(); break;
        case CURL_LOCK_DATA_SHARE:  s_shareMutex.lock(); break;
        default: break; 
    }
}

void CurlAdapter::UnlockCallback(CURL* handle, curl_lock_data data, void* userptr) {
    (void)handle; (void)userptr;
    switch (data) {
        case CURL_LOCK_DATA_COOKIE: s_cookieMutex.unlock(); break;
        case CURL_LOCK_DATA_DNS:    s_dnsMutex.unlock(); break;
        case CURL_LOCK_DATA_SHARE:  s_shareMutex.unlock(); break;
        default: break;
    }
}

size_t CurlAdapter::WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t totalSize = size * nmemb;
    auto* buffer = static_cast<ByteBuffer*>(userp);
    auto* data = static_cast<uint8_t*>(contents);
    buffer->insert(buffer->end(), data, data + totalSize);
    return totalSize;
}

size_t CurlAdapter::HeaderCallback(char* buffer, size_t size, size_t nitems, void* userdata) {
    size_t totalSize = size * nitems;
    std::string line(buffer, totalSize);
    auto* accumulator = static_cast<ResponseHeaderAccumulator*>(userdata);

    // Simple parsing: Look for ':'
    size_t colonPos = line.find(':');
    if (colonPos != std::string::npos) {
        std::string key = line.substr(0, colonPos);
        std::string value = line.substr(colonPos + 1);
        
        // Trim whitespace
        while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) value.erase(0, 1);
        while (!value.empty() && (value.back() == '\r' || value.back() == '\n' || value.back() == ' ')) value.pop_back();

        if (isSetCookieHeader(key) && accumulator->setCookieHeaders != nullptr) {
            accumulator->setCookieHeaders->push_back(value);
        } else if (accumulator->headers != nullptr) {
            (*accumulator->headers)[key] = value;
        }
    }
    return totalSize;
}

std::future<HttpResponse> CurlAdapter::send(const HttpRequest& request) {
    // Launch purely async for now.
    return std::async(std::launch::async, [this, request]() {
        return this->performRequest(request);
    });
}

HttpResponse CurlAdapter::performRequest(const HttpRequest& request) {
    RedirectInfo redirectInfo;
    redirectInfo.finalUrl = request.getUrl();

    std::string currentUrl = request.getUrl();
    HttpMethod currentMethod = request.getMethod();
    ByteBuffer currentBody = request.getBody();
    HeaderMap baseRequestHeaders = request.getHeaders();
    int redirectCount = 0;

    while (true) {
        if (request.isAborted()) {
            HttpResponse response(0, "Request aborted");
            response.setUrl(currentUrl);
            response.setRedirectInfo(redirectInfo);
            return response;
        }

        CURL* curl = curl_easy_init();
        if (!curl) {
            return HttpResponse(500, "Falha ao inicializar CURL");
        }

        if (shouldUseCookieStore(request)) {
            curl_easy_setopt(curl, CURLOPT_SHARE, share_);
            curl_easy_setopt(curl, CURLOPT_COOKIEFILE, "");
        }

        struct curl_slist* headerChunk = nullptr;
        ByteBuffer responseBody;
        HeaderMap responseHeaders;
        std::vector<std::string> responseSetCookieHeaders;
        ResponseHeaderAccumulator headerAccumulator{&responseHeaders, &responseSetCookieHeaders};
        HeaderMap effectiveRequestHeaders = baseRequestHeaders;

        curl_easy_setopt(curl, CURLOPT_URL, currentUrl.c_str());

        // NUNCA usar FOLLOWLOCATION — controlamos cada hop manualmente
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);

        // Método HTTP
        switch (currentMethod) {
            case HttpMethod::GET:
                curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L); break;
            case HttpMethod::POST:
                curl_easy_setopt(curl, CURLOPT_POST, 1L);
                if (!currentBody.empty()) {
                    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, reinterpret_cast<const char*>(currentBody.data()));
                    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(currentBody.size()));
                } else {
                    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, 0L);
                }
                break;
            case HttpMethod::PUT:
                curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
                if (!currentBody.empty()) {
                    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, reinterpret_cast<const char*>(currentBody.data()));
                    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(currentBody.size()));
                }
                break;
            case HttpMethod::DELETE:
                curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE"); break;
            case HttpMethod::PATCH:
                curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PATCH");
                if (!currentBody.empty()) {
                    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, reinterpret_cast<const char*>(currentBody.data()));
                    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(currentBody.size()));
                }
                break;
            case HttpMethod::HEAD:
                curl_easy_setopt(curl, CURLOPT_NOBODY, 1L); break;
            case HttpMethod::OPTIONS:
                curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "OPTIONS"); break;
        }

        // Headers
        if (request.usesBrowserCookieJar() && cookieJar_) {
            std::string cookieHeader = cookieJar_->getCookieHeader(
                buildCookieContext(request, currentUrl, currentMethod));
            if (!cookieHeader.empty()) {
                effectiveRequestHeaders["Cookie"] = cookieHeader;
            }
        }

        for (const auto& [key, value] : effectiveRequestHeaders) {
            std::string headerStr = key + ": " + value;
            headerChunk = curl_slist_append(headerChunk, headerStr.c_str());
        }
        if (headerChunk) {
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerChunk);
        }

        // Timeout
        curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, request.getTimeout());
        ConfigureAbortHandling(curl, request);

        // SSL
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

        // Callbacks
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBody);
        curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, HeaderCallback);
        curl_easy_setopt(curl, CURLOPT_HEADERDATA, &headerAccumulator);

        CURLcode res = curl_easy_perform(curl);

        long responseCode = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);

        curl_slist_free_all(headerChunk);
        curl_easy_cleanup(curl);

        if (res != CURLE_OK) {
            if (request.isAborted()) {
                HttpResponse response(0, "Request aborted");
                response.setUrl(currentUrl);
                response.setRedirectInfo(redirectInfo);
                return response;
            }
            HttpResponse response(static_cast<int>(responseCode), curl_easy_strerror(res));
            response.setUrl(currentUrl);
            response.setRedirectInfo(redirectInfo);
            return response;
        }

        // Verificar se é um redirecionamento (3xx)
        bool isRedirect = (responseCode >= 300 && responseCode < 400);

        if (request.usesBrowserCookieJar() && cookieJar_ && res == CURLE_OK) {
            cookieJar_->storeFromResponse(currentUrl, responseSetCookieHeaders);
        }

        if (!isRedirect) {
            // Resposta final — não é redirect
            redirectInfo.finalUrl = currentUrl;
            HttpResponse response(static_cast<int>(responseCode), "OK");
            response.setBody(std::move(responseBody));
            response.setHeaders(std::move(responseHeaders));
            response.setSetCookieHeaders(std::move(responseSetCookieHeaders));
            response.setUrl(currentUrl);
            response.setRedirectInfo(redirectInfo);
            return response;
        }

        // --- É um redirecionamento ---

        // Modo Error: retorna erro no 3xx
        if (request.getRedirectMode() == RedirectMode::Error) {
            HttpResponse response(static_cast<int>(responseCode), "Redirecionamento proibido (modo Error)");
            response.setHeaders(std::move(responseHeaders));
            response.setSetCookieHeaders(std::move(responseSetCookieHeaders));
            response.setUrl(currentUrl);
            return response;
        }

        // Modo Manual: retorna a resposta 3xx sem seguir
        if (request.getRedirectMode() == RedirectMode::Manual) {
            redirectInfo.finalUrl = currentUrl;
            HttpResponse response(static_cast<int>(responseCode), "Redirecionamento manual");
            response.setBody(std::move(responseBody));
            response.setHeaders(std::move(responseHeaders));
            response.setSetCookieHeaders(std::move(responseSetCookieHeaders));
            response.setUrl(currentUrl);
            response.setRedirectInfo(redirectInfo);
            return response;
        }

        // Modo Follow: seguir o redirecionamento

        // Verificar limite de redirects
        if (redirectCount >= request.getMaxRedirects()) {
            HttpResponse response(static_cast<int>(responseCode), "Limite de redirecionamentos excedido");
            response.setUrl(currentUrl);
            response.setRedirectInfo(redirectInfo);
            return response;
        }

        // Extrair header Location
        std::string location;
        auto it = responseHeaders.find("Location");
        if (it == responseHeaders.end()) {
            it = responseHeaders.find("location");
        }
        if (it != responseHeaders.end()) {
            location = it->second;
        }

        if (location.empty()) {
            // Redirect sem Location — retornar como está
            redirectInfo.finalUrl = currentUrl;
            HttpResponse response(static_cast<int>(responseCode), "Redirecionamento sem Location");
            response.setBody(std::move(responseBody));
            response.setHeaders(std::move(responseHeaders));
            response.setUrl(currentUrl);
            response.setRedirectInfo(redirectInfo);
            return response;
        }

        // Registrar hop na cadeia com headers para DevTools
        RedirectEntry entry;
        entry.url = currentUrl;
        entry.statusCode = static_cast<int>(responseCode);
        entry.method = httpMethodToString(currentMethod);
        entry.requestHeaders = effectiveRequestHeaders;
        entry.responseHeaders = responseHeaders;
        redirectInfo.chain.push_back(std::move(entry));
        redirectInfo.redirected = true;

        // Resolver URL relativa (se a Location for relativa)
        if (location.find("://") == std::string::npos) {
            // URL relativa — resolver baseado na URL atual
            size_t schemeEnd = currentUrl.find("://");
            if (schemeEnd != std::string::npos) {
                if (location[0] == '/') {
                    // Relativa à raiz do host
                    size_t hostEnd = currentUrl.find('/', schemeEnd + 3);
                    if (hostEnd != std::string::npos) {
                        location = currentUrl.substr(0, hostEnd) + normalizePath(location);
                    } else {
                        location = currentUrl + normalizePath(location);
                    }
                } else {
                    // Relativa ao path atual (ex: "../auth", "login")
                    size_t lastSlash = currentUrl.rfind('/');
                    if (lastSlash != std::string::npos && lastSlash > schemeEnd + 2) {
                        std::string basePath = currentUrl.substr(0, lastSlash + 1) + location;
                        // Separar origin do path para normalizar
                        size_t pathStart = basePath.find('/', schemeEnd + 3);
                        if (pathStart != std::string::npos) {
                            std::string origin = basePath.substr(0, pathStart);
                            std::string path = basePath.substr(pathStart);
                            location = origin + normalizePath(path);
                        } else {
                            location = basePath;
                        }
                    } else {
                        location = currentUrl + "/" + location;
                    }
                }
            }
        } else {
            // URL absoluta — normalizar o path se contiver ..
            size_t schemeEnd = location.find("://");
            if (schemeEnd != std::string::npos) {
                size_t pathStart = location.find('/', schemeEnd + 3);
                if (pathStart != std::string::npos) {
                    std::string path = location.substr(pathStart);
                    if (path.find("..") != std::string::npos || path.find("/./") != std::string::npos) {
                        location = location.substr(0, pathStart) + normalizePath(path);
                    }
                }
            }
        }

        /**
         * Segurança: Remoção de headers sensíveis em redirect cross-origin.
         *
         * Quando a origem muda (ex: api.a.com → api.b.com), o navegador
         * remove automaticamente Authorization, Cookie e Proxy-Authorization
         * para evitar vazamento de credenciais entre domínios.
         */
        std::string previousOrigin = extractOrigin(currentUrl);
        std::string nextOrigin = extractOrigin(location);
        if (previousOrigin != nextOrigin) {
            stripSensitiveHeaders(baseRequestHeaders);
        }

        /**
         * Semântica de método conforme RFC 7231 e RFC 7538:
         * - 303 (See Other): SEMPRE muda para GET, descarta body.
         * - 307 (Temporary Redirect): Preserva método e body.
         * - 308 (Permanent Redirect): Preserva método e body.
         * - 301/302: Comportamento histórico dos navegadores — POST → GET, descarta body.
         */
        int code = static_cast<int>(responseCode);
        if (code == 303) {
            currentMethod = HttpMethod::GET;
            currentBody.clear();
        } else if (code == 301 || code == 302) {
            // Comportamento histórico do navegador: POST → GET, body descartado
            if (currentMethod == HttpMethod::POST) {
                currentMethod = HttpMethod::GET;
                currentBody.clear();
            }
        }
        // 307 e 308: método e body preservados (nenhuma ação necessária)

        currentUrl = location;
        redirectCount++;
    }
}

// --- Streaming Implementation ---

struct StreamContext {
    HeaderMap headers;
    std::vector<std::string> setCookieHeaders;
    std::shared_ptr<ReadableStream<ByteBuffer>> bodyStream;
    std::promise<StreamingResponse>* promise;
    bool promiseResolved = false;
    long responseCode = 0;
    CURL* curlHandle = nullptr;
    RedirectMode redirectMode = RedirectMode::Follow;
    std::string currentUrl;
    bool redirected = false;
    bool abortForRedirect = false;
};

size_t CurlAdapter::StreamWriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t totalSize = size * nmemb;
    auto* ctx = static_cast<struct StreamContext*>(userp);
    
    auto* data = static_cast<uint8_t*>(contents);
    ByteBuffer chunk(data, data + totalSize);
    
    try {
        ctx->bodyStream->enqueue(std::move(chunk));
    } catch (...) {
        // Stream foi cancelado pelo consumidor — sinaliza curl para parar
        return 0;
    }
    
    return totalSize;
}

static size_t DiscardWriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    (void)contents;
    (void)userp;
    return size * nmemb;
}

static void ConfigureCurlForRequest(CURL* curl, HttpMethod method, const ByteBuffer& body) {
    switch (method) {
        case HttpMethod::GET:
            curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
            break;
        case HttpMethod::POST:
            curl_easy_setopt(curl, CURLOPT_POST, 1L);
            if (!body.empty()) {
                curl_easy_setopt(curl, CURLOPT_POSTFIELDS, reinterpret_cast<const char*>(body.data()));
                curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
            } else {
                curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, 0L);
            }
            break;
        case HttpMethod::PUT:
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
            if (!body.empty()) {
                curl_easy_setopt(curl, CURLOPT_POSTFIELDS, reinterpret_cast<const char*>(body.data()));
                curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
            }
            break;
        case HttpMethod::DELETE:
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
            break;
        case HttpMethod::PATCH:
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PATCH");
            if (!body.empty()) {
                curl_easy_setopt(curl, CURLOPT_POSTFIELDS, reinterpret_cast<const char*>(body.data()));
                curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
            }
            break;
        case HttpMethod::HEAD:
            curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
            break;
        case HttpMethod::OPTIONS:
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "OPTIONS");
            break;
    }
}

/**
 * @brief Callback de header no modo streaming.
 *
 * Acumula headers e extrai o código de status HTTP da linha de status.
 * Quando detecta uma linha vazia (fim dos headers HTTP), decide se deve
 * resolver a promise (resposta final) ou aguardar (redirect).
 */
static size_t StreamHeaderCallback(char* buffer, size_t size, size_t nitems, void* userdata) {
    size_t totalSize = size * nitems;
    std::string line(buffer, totalSize);
    auto* ctx = static_cast<StreamContext*>(userdata);

    // Detectar linha de status HTTP (ex: "HTTP/1.1 200 OK\r\n")
    if (line.substr(0, 5) == "HTTP/") {
        if (!ctx->promiseResolved) {
            ctx->headers.clear();
        }
        
        // Extrair o código de status
        size_t spacePos = line.find(' ');
        if (spacePos != std::string::npos) {
            std::string codeStr = line.substr(spacePos + 1, 3);
            try {
                ctx->responseCode = std::stol(codeStr);
            } catch (...) {
                ctx->responseCode = 0;
            }
        }
        return totalSize;
    }

    size_t colonPos = line.find(':');
    if (colonPos != std::string::npos) {
        std::string key = line.substr(0, colonPos);
        std::string value = line.substr(colonPos + 1);
        
        while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) value.erase(0, 1);
        while (!value.empty() && (value.back() == '\r' || value.back() == '\n' || value.back() == ' ')) value.pop_back();
        
        if (isSetCookieHeader(key)) {
            ctx->setCookieHeaders.push_back(value);
        } else {
            ctx->headers[key] = value;
        }
    }

    // Linha vazia (\r\n) indica fim dos headers HTTP
    bool isEndOfHeaders = (line == "\r\n" || line == "\n");
    if (isEndOfHeaders && !ctx->promiseResolved) {
        // Fallback: obter response code via curl se não foi parseado
        if (ctx->responseCode == 0 && ctx->curlHandle) {
            curl_easy_getinfo(ctx->curlHandle, CURLINFO_RESPONSE_CODE, &ctx->responseCode);
        }

        bool isRedirect = (ctx->responseCode >= 300 && ctx->responseCode < 400);

        // Lógica de manipulação de Redirect
        if (isRedirect && ctx->redirectMode == RedirectMode::Follow) {
            ctx->abortForRedirect = true;
            return 0;
        }

        if (isRedirect && ctx->redirectMode == RedirectMode::Error) {
             std::string msg = "Network request failed: Redirect detected in error mode";
             StreamingResponse errResp(static_cast<int>(ctx->responseCode), msg, ctx->headers, ctx->bodyStream);
             errResp.setSetCookieHeaders(ctx->setCookieHeaders);
             errResp.setUrl(ctx->currentUrl);
             errResp.setRedirected(ctx->redirected);
             ctx->promise->set_value(std::move(errResp));
             ctx->promiseResolved = true;
             ctx->bodyStream->error(msg);
             return 0;
        }

        ctx->promiseResolved = true;
        
        std::string statusMsg = (ctx->responseCode >= 200 && ctx->responseCode < 300) ? "OK" : "Error";
        StreamingResponse response(
            static_cast<int>(ctx->responseCode),
            statusMsg,
            ctx->headers,
            ctx->bodyStream
        );
        response.setSetCookieHeaders(ctx->setCookieHeaders);
        response.setUrl(ctx->currentUrl);
        response.setRedirected(ctx->redirected);

        ctx->promise->set_value(std::move(response));
    }

    return totalSize;
}

std::future<StreamingResponse> CurlAdapter::sendStreaming(const HttpRequest& request) {
    auto bodyStream = std::make_shared<ReadableStream<ByteBuffer>>(0);
    auto promise = std::make_shared<std::promise<StreamingResponse>>();
    auto future = promise->get_future();

    std::thread([this, request, bodyStream, promise]() mutable {
        std::string currentUrl = request.getUrl();
        HttpMethod currentMethod = request.getMethod();
        ByteBuffer currentBody = request.getBody();
        HeaderMap baseRequestHeaders = request.getHeaders();
        int redirectCount = 0;
        bool redirected = false;

        while (true) {
            if (request.isAborted()) {
                StreamingResponse resp(0, "Request aborted", {}, bodyStream);
                resp.setUrl(currentUrl);
                resp.setRedirected(redirected);
                try {
                    promise->set_value(std::move(resp));
                } catch (...) {}
                bodyStream->error("Request aborted");
                return;
            }

            CURL* curl = curl_easy_init();
            if (!curl) {
                bodyStream->error("Falha ao inicializar CURL");
                try {
                    StreamingResponse errResp(500, "Falha ao inicializar CURL", {}, bodyStream);
                    errResp.setUrl(currentUrl);
                    errResp.setRedirected(redirected);
                    promise->set_value(std::move(errResp));
                } catch (...) {}
                return;
            }

            if (shouldUseCookieStore(request)) {
                curl_easy_setopt(curl, CURLOPT_SHARE, share_);
                curl_easy_setopt(curl, CURLOPT_COOKIEFILE, "");
            }
            curl_easy_setopt(curl, CURLOPT_URL, currentUrl.c_str());
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, request.getTimeout());
            ConfigureAbortHandling(curl, request);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

            ConfigureCurlForRequest(curl, currentMethod, currentBody);

            HeaderMap effectiveRequestHeaders = baseRequestHeaders;
            if (request.usesBrowserCookieJar() && cookieJar_) {
                std::string cookieHeader = cookieJar_->getCookieHeader(
                    buildCookieContext(request, currentUrl, currentMethod));
                if (!cookieHeader.empty()) {
                    effectiveRequestHeaders["Cookie"] = cookieHeader;
                }
            }

            struct curl_slist* headerList = nullptr;
            for (const auto& [key, value] : effectiveRequestHeaders) {
                std::string headerStr = key + ": " + value;
                headerList = curl_slist_append(headerList, headerStr.c_str());
            }
            if (headerList) {
                curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerList);
            }

            StreamContext ctx;
            ctx.bodyStream = bodyStream;
            ctx.promise = promise.get();
            ctx.curlHandle = curl;
            ctx.redirectMode = request.getRedirectMode();
            ctx.currentUrl = currentUrl;
            ctx.redirected = redirected;

            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, StreamWriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ctx);
            curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, StreamHeaderCallback);
            curl_easy_setopt(curl, CURLOPT_HEADERDATA, &ctx);

            CURLcode res = curl_easy_perform(curl);
            long responseCode = 0;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);
            ctx.responseCode = responseCode;

            if (ctx.abortForRedirect) {
                if (request.usesBrowserCookieJar() && cookieJar_) {
                    cookieJar_->storeFromResponse(currentUrl, ctx.setCookieHeaders);
                }
                curl_slist_free_all(headerList);
                curl_easy_cleanup(curl);

                if (redirectCount >= request.getMaxRedirects()) {
                    StreamingResponse resp(
                        static_cast<int>(responseCode),
                        "Limite de redirecionamentos excedido",
                        ctx.headers,
                        bodyStream
                    );
                    resp.setUrl(currentUrl);
                    resp.setRedirected(redirected);
                    try {
                        promise->set_value(std::move(resp));
                    } catch (...) {}
                    bodyStream->close();
                    return;
                }

                std::string location;
                auto it = ctx.headers.find("Location");
                if (it == ctx.headers.end()) {
                    it = ctx.headers.find("location");
                }
                if (it != ctx.headers.end()) {
                    location = it->second;
                }

                if (location.empty()) {
                    StreamingResponse resp(
                        static_cast<int>(responseCode),
                        "Redirecionamento sem Location",
                        ctx.headers,
                        bodyStream
                    );
                    resp.setUrl(currentUrl);
                    resp.setRedirected(redirected);
                    try {
                        promise->set_value(std::move(resp));
                    } catch (...) {}
                    bodyStream->close();
                    return;
                }

                if (location.find("://") == std::string::npos) {
                    size_t schemeEnd = currentUrl.find("://");
                    if (schemeEnd != std::string::npos) {
                        if (!location.empty() && location[0] == '/') {
                            size_t hostEnd = currentUrl.find('/', schemeEnd + 3);
                            if (hostEnd != std::string::npos) {
                                location = currentUrl.substr(0, hostEnd) + normalizePath(location);
                            } else {
                                location = currentUrl + normalizePath(location);
                            }
                        } else {
                            size_t lastSlash = currentUrl.rfind('/');
                            if (lastSlash != std::string::npos && lastSlash > schemeEnd + 2) {
                                std::string basePath = currentUrl.substr(0, lastSlash + 1) + location;
                                size_t pathStart = basePath.find('/', schemeEnd + 3);
                                if (pathStart != std::string::npos) {
                                    std::string origin = basePath.substr(0, pathStart);
                                    std::string path = basePath.substr(pathStart);
                                    location = origin + normalizePath(path);
                                } else {
                                    location = basePath;
                                }
                            } else {
                                location = currentUrl + "/" + location;
                            }
                        }
                    }
                } else {
                    size_t schemeEnd = location.find("://");
                    if (schemeEnd != std::string::npos) {
                        size_t pathStart = location.find('/', schemeEnd + 3);
                        if (pathStart != std::string::npos) {
                            std::string path = location.substr(pathStart);
                            if (path.find("..") != std::string::npos || path.find("/./") != std::string::npos) {
                                location = location.substr(0, pathStart) + normalizePath(path);
                            }
                        }
                    }
                }

                std::string previousOrigin = extractOrigin(currentUrl);
                std::string nextOrigin = extractOrigin(location);
                if (previousOrigin != nextOrigin) {
                    stripSensitiveHeaders(baseRequestHeaders);
                }

                int code = static_cast<int>(responseCode);
                if (code == 303) {
                    currentMethod = HttpMethod::GET;
                    currentBody.clear();
                } else if ((code == 301 || code == 302) && currentMethod == HttpMethod::POST) {
                    currentMethod = HttpMethod::GET;
                    currentBody.clear();
                }

                currentUrl = location;
                redirectCount++;
                redirected = true;
                continue;
            }

            if (res != CURLE_OK) {
                std::string errorMsg =
                    request.isAborted() ? "Request aborted" : curl_easy_strerror(res);
                if (!ctx.promiseResolved) {
                    int errStatus = (responseCode >= 400 || responseCode == 0) ? static_cast<int>(responseCode) : 0;
                    StreamingResponse errResp(
                        errStatus,
                        errorMsg,
                        ctx.headers,
                        bodyStream
                    );
                    errResp.setSetCookieHeaders(ctx.setCookieHeaders);
                    errResp.setUrl(currentUrl);
                    errResp.setRedirected(redirected);
                    try {
                        promise->set_value(std::move(errResp));
                    } catch (...) {}
                }
                bodyStream->error(errorMsg);
            } else {
                if (!ctx.promiseResolved) {
                    if (request.usesBrowserCookieJar() && cookieJar_) {
                        cookieJar_->storeFromResponse(currentUrl, ctx.setCookieHeaders);
                    }
                    StreamingResponse resp(
                        static_cast<int>(responseCode),
                        (responseCode >= 200 && responseCode < 300) ? "OK" : "Error",
                        ctx.headers,
                        bodyStream
                    );
                    resp.setSetCookieHeaders(ctx.setCookieHeaders);
                    resp.setUrl(currentUrl);
                    resp.setRedirected(redirected);
                    try {
                        promise->set_value(std::move(resp));
                    } catch (...) {}
                }
                bodyStream->close();
            }

            curl_slist_free_all(headerList);
            curl_easy_cleanup(curl);
            return;
        }
    }).detach();

    return future;
}

} // namespace browser_network
