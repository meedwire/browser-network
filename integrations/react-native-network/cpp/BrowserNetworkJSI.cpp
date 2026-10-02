#include "BrowserNetworkJSI.h"
#include <jsi/jsi.h>
#include "adapters/cache/CachedNetworkAdapter.h"
#include "adapters/curl/CurlAdapter.h"
#include "adapters/security/SecurityAdapter.h"
#include "adapters/cache/InMemoryCacheStorage.h"
#include "core/cache/HttpCache.h"
#include "core/security/NetworkConfig.h"
#include "core/security/SecurityPolicy.h"
#include "core/HttpRequest.h"
#include "core/HttpResponse.h"
#include "core/StreamingResponse.h"
#include "core/RedirectTypes.h" 
#include "WebSocketJSI.h"
#include "observers/NetworkObserverAdapter.h"
#include "observers/INetworkObserver.h"
#include "observers/InspectorNetworkObserver.h"
#ifndef __ANDROID__
#include "ReactNativeNetworkInspector.h"
#endif
#include "observers/JsiNetworkObserver.h"
#include <string>
#include <thread>
#include <memory>
#include <vector>
#include <atomic>
#include <algorithm>
#include <cctype>
#include <sstream>
#include <map>
#include <mutex>

#ifdef __ANDROID__
#include <android/log.h>
#define LOG_TAG "BrowserNetworkJSI"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#else
#define LOGD(...) printf(__VA_ARGS__)
#define LOGE(...) fprintf(stderr, __VA_ARGS__)
#endif

namespace browsernetwork {

using namespace facebook;
using namespace browser_network;

// Armazenamos o Runtime e o CallInvoker no JSI de forma segura
static jsi::Runtime* g_jsRuntime = nullptr;

// Instância global do SecurityAdapter (ou do stack de adapters)
static std::shared_ptr<INetworkAdapter> g_networkStack;
static std::shared_ptr<CookieJar> g_cookieJar;
static std::optional<Origin> g_effectiveOrigin;
static std::mutex g_stackMutex;

/**
 * @brief Base64 Encoder Implementation
 * Used to safely transfer binary data from C++ to JS via JSI String.
 */
static const std::string base64_chars =
             "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
             "abcdefghijklmnopqrstuvwxyz"
             "0123456789+/";

static std::string base64_encode(const unsigned char* bytes_to_encode, unsigned int in_len) {
  std::string ret;
  int i = 0;
  int j = 0;
  unsigned char char_array_3[3];
  unsigned char char_array_4[4];

  while (in_len--) {
    char_array_3[i++] = *(bytes_to_encode++);
    if (i == 3) {
      char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
      char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
      char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
      char_array_4[3] = char_array_3[2] & 0x3f;

      for(i = 0; (i <4) ; i++)
        ret += base64_chars[char_array_4[i]];
      i = 0;
    }
  }

  if (i) {
    for(j = i; j < 3; j++)
      char_array_3[j] = '\0';

    char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
    char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
    char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
    char_array_4[3] = char_array_3[2] & 0x3f;

    for (j = 0; (j < i + 1); j++)
      ret += base64_chars[char_array_4[j]];

    while((i++ < 3))
      ret += '=';
  }

  return ret;
}

static inline bool is_base64(unsigned char c) {
  return (std::isalnum(c) || (c == '+') || (c == '/'));
}

static ByteBuffer base64_decode(const std::string& encoded_string) {
  int in_len = static_cast<int>(encoded_string.size());
  int i = 0;
  int j = 0;
  int in_ = 0;
  unsigned char char_array_4[4], char_array_3[3];
  ByteBuffer ret;

  while (in_len-- && (encoded_string[in_] != '=') && is_base64(encoded_string[in_])) {
    char_array_4[i++] = encoded_string[in_];
    in_++;
    if (i == 4) {
      for (i = 0; i < 4; i++) {
        char_array_4[i] = static_cast<unsigned char>(base64_chars.find(char_array_4[i]));
      }

      char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
      char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
      char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

      for (i = 0; i < 3; i++) {
        ret.push_back(char_array_3[i]);
      }
      i = 0;
    }
  }

  if (i) {
    for (j = i; j < 4; j++) {
      char_array_4[j] = 0;
    }

    for (j = 0; j < 4; j++) {
      char_array_4[j] = static_cast<unsigned char>(base64_chars.find(char_array_4[j]));
    }

    char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
    char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
    char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

    for (j = 0; j < i - 1; j++) {
      ret.push_back(char_array_3[j]);
    }
  }

  return ret;
}

/**
 * @brief Estrutura para gerenciar conexões SSE ativas.
 */
struct SSEConnection {
    std::atomic<bool> active{true};
    std::shared_ptr<std::atomic<bool>> abortSignal{std::make_shared<std::atomic<bool>>(false)};
    std::shared_ptr<ReadableStream<ByteBuffer>> stream;
};

static std::mutex g_sseConnectionsMutex;
static std::map<int, std::shared_ptr<SSEConnection>> g_sseConnections;
static std::atomic<int> g_sseConnectionIdCounter{0};

struct StreamRequestConnection {
    std::shared_ptr<std::atomic<bool>> abortSignal{std::make_shared<std::atomic<bool>>(false)};
    std::shared_ptr<ReadableStream<ByteBuffer>> stream;
};

static std::mutex g_streamRequestsMutex;
static std::map<int, std::shared_ptr<StreamRequestConnection>> g_streamRequests;
static std::atomic<int> g_streamRequestIdCounter{0};

static INetworkAdapter& getNetworkStack() {
    std::lock_guard<std::mutex> lock(g_stackMutex);
    if (!g_networkStack) {
        LOGE("Network stack not initialized! Calling getNetworkStack before install()");
        auto curl = std::make_shared<CurlAdapter>();
        auto cacheStorage = std::make_shared<InMemoryCacheStorage>(100);
        auto httpCache = std::make_shared<HttpCache>(cacheStorage);
        auto cachedAdapter = std::make_shared<CachedNetworkAdapter>(curl, httpCache);
        NetworkConfig config;
        config.mode = OriginMode::BrowserEmulation;
        config.bundleId = "com.default.app";
        auto securityAdapter = std::make_shared<SecurityAdapter>(cachedAdapter, config);
        curl->setCookieJar(securityAdapter->getCookieJar());
        g_cookieJar = securityAdapter->getCookieJar();
        g_effectiveOrigin = SecurityPolicy::resolveOrigin(config);
        g_networkStack = securityAdapter;
    }
    return *g_networkStack;
}

/**
 * @brief Parseia um evento SSE a partir de um buffer acumulado.
 */
bool parseSSEEvent(std::string& buffer, std::string& eventType, std::string& eventId,
                   std::string& eventData, int& retryMs) {
    size_t eventEnd = buffer.find("\n\n");
    if (eventEnd == std::string::npos) {
        return false;
    }

    std::string eventBlock = buffer.substr(0, eventEnd);
    buffer = buffer.substr(eventEnd + 2);

    eventType = "message"; // Tipo padrão
    eventId.clear();
    eventData.clear();
    retryMs = -1;

    std::istringstream stream(eventBlock);
    std::string line;
    std::vector<std::string> dataLines;

    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        if (line.empty() || line[0] == ':') {
            continue;
        }

        size_t colonPos = line.find(':');
        if (colonPos == std::string::npos) {
            continue;
        }

        std::string field = line.substr(0, colonPos);
        std::string value = line.substr(colonPos + 1);

        if (!value.empty() && value[0] == ' ') {
            value = value.substr(1);
        }

        if (field == "event") {
            eventType = value;
        } else if (field == "id") {
            eventId = value;
        } else if (field == "data") {
            dataLines.push_back(value);
        } else if (field == "retry") {
            try {
                retryMs = std::stoi(value);
            } catch (...) {
                retryMs = -1;
            }
        }
    }

    for (size_t i = 0; i < dataLines.size(); i++) {
        eventData += dataLines[i];
        if (i < dataLines.size() - 1) {
            eventData += "\n";
        }
    }

    return !eventData.empty() || retryMs >= 0;
}

// Helper para converter string para HttpMethod
HttpMethod stringToHttpMethod(const std::string& method) {
    if (method == "GET") return HttpMethod::GET;
    if (method == "POST") return HttpMethod::POST;
    if (method == "PUT") return HttpMethod::PUT;
    if (method == "DELETE") return HttpMethod::DELETE;
    if (method == "PATCH") return HttpMethod::PATCH;
    if (method == "HEAD") return HttpMethod::HEAD;
    if (method == "OPTIONS") return HttpMethod::OPTIONS;
    return HttpMethod::GET; // default
}

CredentialsMode stringToCredentialsMode(const std::string& mode) {
    if (mode == "omit") return CredentialsMode::Omit;
    if (mode == "same-origin") return CredentialsMode::SameOrigin;
    return CredentialsMode::Include;
}

std::string getHeaderValueCaseInsensitive(const HeaderMap& headers, const std::string& name) {
    for (const auto& [key, value] : headers) {
        if (key.size() != name.size()) {
            continue;
        }

        bool match = true;
        for (size_t i = 0; i < key.size(); ++i) {
            if (std::tolower(static_cast<unsigned char>(key[i])) !=
                std::tolower(static_cast<unsigned char>(name[i]))) {
                match = false;
                break;
            }
        }

        if (match) {
            return value;
        }
    }

    return "";
}

bool isEventStreamContentType(const HeaderMap& headers) {
    std::string contentType = getHeaderValueCaseInsensitive(headers, "content-type");
    if (contentType.empty()) {
        return false;
    }

    std::string normalized = contentType;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    return normalized.rfind("text/event-stream", 0) == 0;
}

void install(jsi::Runtime &jsiRuntime,
             std::shared_ptr<react::CallInvoker> jsCallInvoker,
             const std::string& bundleId,
             const std::string& cookieStoragePath) {
    LOGD("Installing BrowserNetwork JSI module with bundleId: %s", bundleId.c_str());
    
    // Inicializar o stack de rede com segurança e ferramentas de debug
    {
        std::lock_guard<std::mutex> lock(g_stackMutex);
        
        // 1. Base Adapter (Curl)
        auto baseAdapter = std::make_shared<CurlAdapter>();
        
        // 2. Cache Adapter
        auto cacheStorage = std::make_shared<InMemoryCacheStorage>(100);
        auto httpCache = std::make_shared<HttpCache>(cacheStorage);
        auto cachedAdapter = std::make_shared<CachedNetworkAdapter>(baseAdapter, httpCache);

        // 3. Security Adapter (CORS / Origin)
        NetworkConfig config;
        config.mode = OriginMode::BrowserEmulation;
        config.bundleId = bundleId;
        config.cookiePersistencePath = cookieStoragePath;
        auto securityAdapter = std::make_shared<SecurityAdapter>(cachedAdapter, config);
        baseAdapter->setCookieJar(securityAdapter->getCookieJar());
        g_cookieJar = securityAdapter->getCookieJar();
        g_effectiveOrigin = SecurityPolicy::resolveOrigin(config);
        
        g_networkStack = securityAdapter;
#ifndef __ANDROID__
        auto rnInspector = std::make_shared<ReactNativeNetworkInspector>();
        auto devToolsObserver =
            std::make_shared<InspectorNetworkObserver>(rnInspector);
        g_networkStack = std::make_shared<NetworkObserverAdapter>(
            securityAdapter, devToolsObserver);
#endif
    }

    g_jsRuntime = &jsiRuntime;
    auto browserNetworkModule = jsi::Object(jsiRuntime);
    LOGD("BrowserNetwork JSI module object created");

    auto setNetworkObserver = jsi::Function::createFromHostFunction(
        jsiRuntime,
        jsi::PropNameID::forAscii(jsiRuntime, "setNetworkObserver"),
        1,
        [jsCallInvoker](jsi::Runtime &runtime, const jsi::Value &thisValue, const jsi::Value *arguments, size_t count) -> jsi::Value {
            if (count < 1 || !arguments[0].isObject() || !arguments[0].asObject(runtime).isFunction(runtime)) {
                return jsi::Value::undefined();
            }

            auto callback = arguments[0].asObject(runtime).asFunction(runtime);
            auto jsiObserver = std::make_shared<JsiNetworkObserver>(runtime, jsCallInvoker, std::move(callback));
            
            // Atualiza o stack de rede para usar o novo observador JSI
            // Podemos manter o DevTools ou outros se quisermos compor, 
            // mas aqui substituímos para dar controle total ao usuário.
            std::lock_guard<std::mutex> lock(g_stackMutex);
            if (g_networkStack) {
                // Aqui uma decisão: o NetworkObserverAdapter deveria suportar múltiplos observers?
                // Por agora, apenas substituímos o observer do adaptador existente ou criamos um novo decorador
                // Para simplificar e garantir a arquitetura, vamos assumir que o usuário quer controlar.
                
                // Nota: se g_networkStack já for um NetworkObserverAdapter, podemos apenas trocar o observer_ interno
                // Mas o observer_ é private. Vamos reconstruir o stack para simplificar.
                
                // Buscamos o adaptador original (inner)
                // Assumindo que o stack atual é: SecurityAdapter -> CurlAdapter
                // Ou: NetworkObserverAdapter -> SecurityAdapter -> CurlAdapter
                
                // Para manter a segurança, vamos apenas re-anexar o observer.
                // Idealmente teríamos uma forma de compor.
                // Vamos criar uma nova instância do adaptador com o novo observer.
                // PERIGO: Isso pode perder o estado de requisições em curso se não for cuidadoso.
                // Mas como g_networkStack é um shared_ptr, as requisições antigas continuam vivas no objeto antigo.
                
                // Pegamos o baseAdapter atual (que deveria ser o securityAdapter ou curlAdapter)
                // Como não temos acesso fácil ao "inner" sem cast, vamos assumir que queremos 
                // envolver o stack atual com o novo observador.
                g_networkStack = std::make_shared<NetworkObserverAdapter>(g_networkStack, jsiObserver);
            }
            
            return jsi::Value::undefined();
        }
    );
    browserNetworkModule.setProperty(jsiRuntime, "setNetworkObserver", std::move(setNetworkObserver));

    auto nativePerformRequest = jsi::Function::createFromHostFunction(
        jsiRuntime,
        jsi::PropNameID::forAscii(jsiRuntime, "nativePerformRequest"),
        2,
        [jsCallInvoker](jsi::Runtime &runtime, const jsi::Value &thisValue, const jsi::Value *arguments, size_t count) -> jsi::Value {
            if (count < 2 || !arguments[0].isString() || !arguments[1].isObject()) {
                return jsi::Value::undefined();
            }

            std::string url = arguments[0].asString(runtime).utf8(runtime);
            jsi::Object options = arguments[1].asObject(runtime);
            std::string methodStr = "GET";
            if (options.hasProperty(runtime, "method")) {
                jsi::Value methodValue = options.getProperty(runtime, "method");
                if (methodValue.isString()) {
                    methodStr = methodValue.asString(runtime).utf8(runtime);
                }
            }
            HttpMethod method = stringToHttpMethod(methodStr);

            HeaderMap headers;
            if (options.hasProperty(runtime, "headers")) {
                jsi::Value headersValue = options.getProperty(runtime, "headers");
                if (headersValue.isObject()) {
                    jsi::Object headersObj = headersValue.asObject(runtime);
                    jsi::Array propertyNames = headersObj.getPropertyNames(runtime);
                    size_t length = propertyNames.size(runtime);
                    for (size_t i = 0; i < length; i++) {
                        jsi::String keyStr = propertyNames.getValueAtIndex(runtime, i).asString(runtime);
                        std::string key = keyStr.utf8(runtime);
                        jsi::Value value = headersObj.getProperty(runtime, keyStr);
                        if (value.isString()) {
                            headers[key] = value.asString(runtime).utf8(runtime);
                        }
                    }
                }
            }

            std::string bodyStr;
            if (options.hasProperty(runtime, "body")) {
                 jsi::Value bodyValue = options.getProperty(runtime, "body");
                 if (bodyValue.isString()) {
                     bodyStr = bodyValue.asString(runtime).utf8(runtime);
                 }
            }
            bool bodyBase64 = false;
            if (options.hasProperty(runtime, "bodyBase64")) {
                jsi::Value bodyBase64Value = options.getProperty(runtime, "bodyBase64");
                if (bodyBase64Value.isBool()) {
                    bodyBase64 = bodyBase64Value.getBool();
                }
            }

            std::string redirectStr = "follow";
            if (options.hasProperty(runtime, "redirect")) {
                jsi::Value val = options.getProperty(runtime, "redirect");
                if (val.isString()) redirectStr = val.asString(runtime).utf8(runtime);
            }
            RedirectMode redirectMode = RedirectMode::Follow;
            if (redirectStr == "error") redirectMode = RedirectMode::Error;
            else if (redirectStr == "manual") redirectMode = RedirectMode::Manual;

            std::string credentialsStr = "include";
            if (options.hasProperty(runtime, "credentials")) {
                jsi::Value val = options.getProperty(runtime, "credentials");
                if (val.isString()) credentialsStr = val.asString(runtime).utf8(runtime);
            }
            CredentialsMode credentialsMode = stringToCredentialsMode(credentialsStr);

            jsi::Function onResponse = options.getProperty(runtime, "onResponse").asObject(runtime).asFunction(runtime);
            jsi::Function onError = options.getProperty(runtime, "onError").asObject(runtime).asFunction(runtime);
            auto sharedOnResponse = std::make_shared<jsi::Value>(runtime, std::move(onResponse));
            auto sharedOnError = std::make_shared<jsi::Value>(runtime, std::move(onError));

            std::thread([jsCallInvoker, url, method, headers, bodyStr, bodyBase64, redirectMode, credentialsMode, sharedOnResponse, sharedOnError]() {
                try {
                    INetworkAdapter& adapter = getNetworkStack();
                    HttpRequest request(url, method);
                    request.setRedirectMode(redirectMode);
                    request.setCredentialsMode(credentialsMode);
                    for (const auto& [key, value] : headers) request.addHeader(key, value);
                    if (!bodyStr.empty()) {
                        if (bodyBase64) request.setBody(base64_decode(bodyStr));
                        else request.setBody(bodyStr);
                    }

                    auto responseFuture = adapter.send(request);
                    auto response = responseFuture.get();

                    jsCallInvoker->invokeAsync([sharedOnResponse, response]() {
                        try {
                            if (!g_jsRuntime) return;
                            jsi::Runtime& rt = *g_jsRuntime;
                            jsi::Object jsResponse(rt);
                            jsResponse.setProperty(rt, "status", (int)response.getStatus().code);
                            jsResponse.setProperty(rt, "url", jsi::String::createFromUtf8(rt, response.getUrl()));
                            
                            std::string bodyStr(reinterpret_cast<const char*>(response.getBody().data()), response.getBody().size());
                            jsResponse.setProperty(rt, "body", jsi::String::createFromUtf8(rt, bodyStr));

                            jsi::Object jsHeaders(rt);
                            for (const auto& [key, value] : response.getHeaders()) {
                                jsHeaders.setProperty(rt, key.c_str(), jsi::String::createFromUtf8(rt, value));
                            }
                            jsResponse.setProperty(rt, "headers", std::move(jsHeaders));
                            sharedOnResponse->asObject(rt).asFunction(rt).call(rt, std::move(jsResponse));
                        } catch (const std::exception& e) {
                            LOGE("Error in nativePerformRequest callback: %s", e.what());
                        } catch (...) {
                            LOGE("Unknown error in nativePerformRequest callback");
                        }
                    });
                } catch (const std::exception& e) {
                    std::string errorMsg = e.what();
                    jsCallInvoker->invokeAsync([sharedOnError, errorMsg]() {
                        if (!g_jsRuntime) return;
                        jsi::Runtime& rt = *g_jsRuntime;
                        jsi::String jsError = jsi::String::createFromUtf8(rt, errorMsg);
                        sharedOnError->asObject(rt).asFunction(rt).call(rt, std::move(jsError));
                    });
                } catch (...) {
                    jsCallInvoker->invokeAsync([sharedOnError]() {
                        if (!g_jsRuntime) return;
                        jsi::Runtime& rt = *g_jsRuntime;
                        jsi::String jsError = jsi::String::createFromUtf8(rt, "Unknown error");
                        sharedOnError->asObject(rt).asFunction(rt).call(rt, std::move(jsError));
                    });
                }
            }).detach();

            return jsi::Value::undefined();
        });

    auto nativePerformStreamRequest = jsi::Function::createFromHostFunction(
        jsiRuntime,
        jsi::PropNameID::forAscii(jsiRuntime, "nativePerformStreamRequest"),
        2,
        [jsCallInvoker](jsi::Runtime &runtime, const jsi::Value &thisValue, const jsi::Value *arguments, size_t count) -> jsi::Value {
            if (count < 2 || !arguments[0].isString() || !arguments[1].isObject()) {
                return jsi::Value::undefined();
            }

            std::string url = arguments[0].asString(runtime).utf8(runtime);
            jsi::Object options = arguments[1].asObject(runtime);
            
            std::string methodStr = "GET";
            if (options.hasProperty(runtime, "method")) {
                jsi::Value methodValue = options.getProperty(runtime, "method");
                if (methodValue.isString()) methodStr = methodValue.asString(runtime).utf8(runtime);
            }
            HttpMethod method = stringToHttpMethod(methodStr);

            HeaderMap headers;
            if (options.hasProperty(runtime, "headers")) {
                jsi::Value headersValue = options.getProperty(runtime, "headers");
                if (headersValue.isObject()) {
                    jsi::Object headersObj = headersValue.asObject(runtime);
                    jsi::Array propertyNames = headersObj.getPropertyNames(runtime);
                    size_t length = propertyNames.size(runtime);
                    for (size_t i = 0; i < length; i++) {
                        jsi::String keyStr = propertyNames.getValueAtIndex(runtime, i).asString(runtime);
                        std::string key = keyStr.utf8(runtime);
                        jsi::Value value = headersObj.getProperty(runtime, keyStr);
                        if (value.isString()) headers[key] = value.asString(runtime).utf8(runtime);
                    }
                }
            }
            
            std::string bodyStr;
            if (options.hasProperty(runtime, "body")) {
                 jsi::Value bodyValue = options.getProperty(runtime, "body");
                 if (bodyValue.isString()) {
                     bodyStr = bodyValue.asString(runtime).utf8(runtime);
                 }
            }
            bool bodyBase64 = false;
            if (options.hasProperty(runtime, "bodyBase64")) {
                jsi::Value bodyBase64Value = options.getProperty(runtime, "bodyBase64");
                if (bodyBase64Value.isBool()) {
                    bodyBase64 = bodyBase64Value.getBool();
                }
            }

            std::string redirectStr = "follow";
            if (options.hasProperty(runtime, "redirect")) {
                jsi::Value val = options.getProperty(runtime, "redirect");
                if (val.isString()) redirectStr = val.asString(runtime).utf8(runtime);
            }
            RedirectMode redirectMode = RedirectMode::Follow;
            if (redirectStr == "error") redirectMode = RedirectMode::Error;
            else if (redirectStr == "manual") redirectMode = RedirectMode::Manual;

            std::string credentialsStr = "include";
            if (options.hasProperty(runtime, "credentials")) {
                jsi::Value val = options.getProperty(runtime, "credentials");
                if (val.isString()) credentialsStr = val.asString(runtime).utf8(runtime);
            }
            CredentialsMode credentialsMode = stringToCredentialsMode(credentialsStr);

            jsi::Function onResponse = options.getProperty(runtime, "onResponse").asObject(runtime).asFunction(runtime);
            jsi::Function onData = options.getProperty(runtime, "onData").asObject(runtime).asFunction(runtime);
            jsi::Function onEnd = options.getProperty(runtime, "onEnd").asObject(runtime).asFunction(runtime);
            jsi::Function onError = options.getProperty(runtime, "onError").asObject(runtime).asFunction(runtime);

            auto sharedOnResponse = std::make_shared<jsi::Value>(runtime, std::move(onResponse));
            auto sharedOnData = std::make_shared<jsi::Value>(runtime, std::move(onData));
            auto sharedOnEnd = std::make_shared<jsi::Value>(runtime, std::move(onEnd));
            auto sharedOnError = std::make_shared<jsi::Value>(runtime, std::move(onError));

            auto connection = std::make_shared<StreamRequestConnection>();
            int requestId = g_streamRequestIdCounter.fetch_add(1);
            {
                std::lock_guard<std::mutex> lock(g_streamRequestsMutex);
                g_streamRequests[requestId] = connection;
            }

            std::thread([jsCallInvoker, url, method, headers, bodyStr, bodyBase64, redirectMode, credentialsMode, connection, requestId, sharedOnResponse, sharedOnData, sharedOnEnd, sharedOnError]() {
                try {
                    LOGD("Stream thread started for URL: %s", url.c_str());
                    INetworkAdapter& adapter = getNetworkStack();
                    HttpRequest request(url, method);
                    request.setRedirectMode(redirectMode);
                    request.setCredentialsMode(credentialsMode);
                    request.setAbortSignal(connection->abortSignal);
                    for (const auto& [key, value] : headers) request.addHeader(key, value);
                    if (!bodyStr.empty()) {
                        if (bodyBase64) request.setBody(base64_decode(bodyStr));
                        else request.setBody(bodyStr);
                    }

                    auto responseFuture = adapter.sendStreaming(request);
                    auto response = responseFuture.get();
                    connection->stream = response.getBody();

                    if (connection->abortSignal->load()) {
                        if (connection->stream) {
                            connection->stream->cancel();
                        }
                        std::lock_guard<std::mutex> lock(g_streamRequestsMutex);
                        g_streamRequests.erase(requestId);
                        return;
                    }

                    jsCallInvoker->invokeAsync([sharedOnResponse, status = response.getStatus().code, headers = response.getHeaders(), finalUrl = response.getUrl(), redirected = response.redirected()]() {
                        try {
                            if (!g_jsRuntime) return;
                            jsi::Runtime& rt = *g_jsRuntime;
                            
                            jsi::Object jsResponse(rt);
                            jsResponse.setProperty(rt, "status", (int)status);
                            jsResponse.setProperty(rt, "url", jsi::String::createFromUtf8(rt, finalUrl));
                            jsResponse.setProperty(rt, "redirected", redirected);
                            
                            jsi::Object jsHeaders(rt);
                            for (const auto& [key, value] : headers) {
                                jsHeaders.setProperty(rt, key.c_str(), jsi::String::createFromUtf8(rt, value));
                            }
                            jsResponse.setProperty(rt, "headers", std::move(jsHeaders));
                            
                            sharedOnResponse->asObject(rt).asFunction(rt).call(rt, std::move(jsResponse));
                        } catch (const std::exception& e) {
                            LOGE("Error in nativePerformStreamRequest onResponse: %s", e.what());
                        } catch (...) {
                            LOGE("Unknown error in nativePerformStreamRequest onResponse");
                        }
                    });

                    auto bodyStream = connection->stream;
                    while (true) {
                        auto chunk = bodyStream->read();
                        if (!chunk) break;

                        std::vector<uint8_t> dataCopy(chunk->begin(), chunk->end());
                        // Use Base64 encoding to clear binary data safely
                        std::string base64Data = base64_encode(dataCopy.data(), dataCopy.size());

                        jsCallInvoker->invokeAsync([sharedOnData, base64Data = std::move(base64Data)]() {
                            try {
                                if (!g_jsRuntime) return;
                                jsi::Runtime& rt = *g_jsRuntime;
                                sharedOnData->asObject(rt).asFunction(rt).call(rt, jsi::String::createFromUtf8(rt, base64Data));
                            } catch (const std::exception& e) {
                                LOGE("Error in nativePerformStreamRequest onData: %s", e.what());
                            } catch (...) {
                                LOGE("Unknown error in nativePerformStreamRequest onData");
                            }
                        });
                    }

                    {
                        std::lock_guard<std::mutex> lock(g_streamRequestsMutex);
                        g_streamRequests.erase(requestId);
                    }

                    jsCallInvoker->invokeAsync([sharedOnEnd]() {
                        if (!g_jsRuntime) return;
                        jsi::Runtime& rt = *g_jsRuntime;
                        sharedOnEnd->asObject(rt).asFunction(rt).call(rt);
                    });

                } catch (const std::exception& e) {
                    {
                        std::lock_guard<std::mutex> lock(g_streamRequestsMutex);
                        g_streamRequests.erase(requestId);
                    }
                    if (connection->abortSignal->load()) {
                        return;
                    }
                    LOGE("Exception in stream thread: %s", e.what());
                    jsCallInvoker->invokeAsync([sharedOnError, errorMsg = std::string(e.what())]() {
                        if (!g_jsRuntime) return;
                        jsi::Runtime& rt = *g_jsRuntime;
                        sharedOnError->asObject(rt).asFunction(rt).call(rt, jsi::String::createFromUtf8(rt, errorMsg));
                    });
                }
            }).detach();

            return jsi::Value(requestId);
        });

    auto nativeAbortStreamRequest = jsi::Function::createFromHostFunction(
        jsiRuntime,
        jsi::PropNameID::forAscii(jsiRuntime, "nativeAbortStreamRequest"),
        1,
        [](jsi::Runtime &runtime, const jsi::Value &thisValue, const jsi::Value *arguments, size_t count) -> jsi::Value {
            (void)runtime;
            (void)thisValue;
            if (count < 1 || !arguments[0].isNumber()) {
                return jsi::Value(false);
            }

            int requestId = static_cast<int>(arguments[0].asNumber());
            std::shared_ptr<StreamRequestConnection> connection;
            {
                std::lock_guard<std::mutex> lock(g_streamRequestsMutex);
                auto it = g_streamRequests.find(requestId);
                if (it == g_streamRequests.end()) {
                    return jsi::Value(false);
                }
                connection = it->second;
                g_streamRequests.erase(it);
            }

            connection->abortSignal->store(true);
            if (connection->stream) {
                connection->stream->cancel();
            }

            return jsi::Value(true);
        });

    auto nativeSSEConnect = jsi::Function::createFromHostFunction(
        jsiRuntime,
        jsi::PropNameID::forAscii(jsiRuntime, "nativeSSEConnect"),
        2,
        [jsCallInvoker](jsi::Runtime &runtime, const jsi::Value &thisValue, const jsi::Value *arguments, size_t count) -> jsi::Value {
            if (count < 2 || !arguments[0].isString() || !arguments[1].isObject()) return jsi::Value(-1);

            std::string url = arguments[0].asString(runtime).utf8(runtime);
            jsi::Object options = arguments[1].asObject(runtime);

            HeaderMap headers;
            if (options.hasProperty(runtime, "headers")) {
                jsi::Value headersValue = options.getProperty(runtime, "headers");
                if (headersValue.isObject()) {
                    jsi::Object headersObj = headersValue.asObject(runtime);
                    jsi::Array propertyNames = headersObj.getPropertyNames(runtime);
                    size_t length = propertyNames.size(runtime);
                    for (size_t i = 0; i < length; i++) {
                        jsi::String keyStr = propertyNames.getValueAtIndex(runtime, i).asString(runtime);
                        std::string key = keyStr.utf8(runtime);
                        jsi::Value value = headersObj.getProperty(runtime, keyStr);
                        if (value.isString()) headers[key] = value.asString(runtime).utf8(runtime);
                    }
                }
            }
            headers["Accept"] = "text/event-stream";

            bool withCredentials = false;
            if (options.hasProperty(runtime, "withCredentials")) {
                jsi::Value val = options.getProperty(runtime, "withCredentials");
                if (val.isBool()) withCredentials = val.getBool();
            }

            auto sharedOnOpen = std::make_shared<jsi::Value>(jsi::Value::undefined());
            auto sharedOnMessage = std::make_shared<jsi::Value>(jsi::Value::undefined());
            auto sharedOnError = std::make_shared<jsi::Value>(jsi::Value::undefined());
            auto sharedOnRetry = std::make_shared<jsi::Value>(jsi::Value::undefined());

            if (options.hasProperty(runtime, "onOpen")) {
                jsi::Value val = options.getProperty(runtime, "onOpen");
                if (val.isObject() && val.asObject(runtime).isFunction(runtime)) *sharedOnOpen = jsi::Value(runtime, val.asObject(runtime).asFunction(runtime));
            }
            if (options.hasProperty(runtime, "onMessage")) {
                jsi::Value val = options.getProperty(runtime, "onMessage");
                if (val.isObject() && val.asObject(runtime).isFunction(runtime)) *sharedOnMessage = jsi::Value(runtime, val.asObject(runtime).asFunction(runtime));
            }
            if (options.hasProperty(runtime, "onError")) {
                jsi::Value val = options.getProperty(runtime, "onError");
                if (val.isObject() && val.asObject(runtime).isFunction(runtime)) *sharedOnError = jsi::Value(runtime, val.asObject(runtime).asFunction(runtime));
            }
            if (options.hasProperty(runtime, "onRetry")) {
                jsi::Value val = options.getProperty(runtime, "onRetry");
                if (val.isObject() && val.asObject(runtime).isFunction(runtime)) *sharedOnRetry = jsi::Value(runtime, val.asObject(runtime).asFunction(runtime));
            }

            auto connection = std::make_shared<SSEConnection>();
            int connectionId = g_sseConnectionIdCounter.fetch_add(1);
            {
                std::lock_guard<std::mutex> lock(g_sseConnectionsMutex);
                g_sseConnections[connectionId] = connection;
            }

            std::thread([jsCallInvoker, url, headers, withCredentials, connection, connectionId, sharedOnOpen, sharedOnMessage, sharedOnError, sharedOnRetry]() {
                try {
                    INetworkAdapter& adapter = getNetworkStack();
                    HttpRequest request(url, HttpMethod::GET);
                    request.setAbortSignal(connection->abortSignal);
                    request.setCredentialsMode(withCredentials ? CredentialsMode::Include : CredentialsMode::SameOrigin);
                    for (const auto& [key, value] : headers) request.addHeader(key, value);
                    request.setTimeout(1800000); // 30 min

                    auto streamingFuture = adapter.sendStreaming(request);
                    auto streamingResponse = streamingFuture.get();

                    int status = streamingResponse.getStatus().code;
                    if (status != 200) {
                        std::string errorMsg = "SSE connection failed with status: " + std::to_string(status);
                        jsCallInvoker->invokeAsync([sharedOnError, errorMsg]() {
                            if (!g_jsRuntime) return;
                            jsi::Runtime& rt = *g_jsRuntime;
                            if (sharedOnError->isObject()) {
                                jsi::Object errorObj(rt);
                                errorObj.setProperty(rt, "message", jsi::String::createFromUtf8(rt, errorMsg));
                                sharedOnError->asObject(rt).asFunction(rt).call(rt, std::move(errorObj));
                            }
                        });
                        {
                            std::lock_guard<std::mutex> lock(g_sseConnectionsMutex);
                            g_sseConnections.erase(connectionId);
                        }
                        return;
                    }

                    if (!isEventStreamContentType(streamingResponse.getHeaders())) {
                        std::string errorMsg = "SSE connection failed: expected Content-Type text/event-stream";
                        jsCallInvoker->invokeAsync([sharedOnError, errorMsg]() {
                            if (!g_jsRuntime) return;
                            jsi::Runtime& rt = *g_jsRuntime;
                            if (sharedOnError->isObject()) {
                                jsi::Object errorObj(rt);
                                errorObj.setProperty(rt, "message", jsi::String::createFromUtf8(rt, errorMsg));
                                sharedOnError->asObject(rt).asFunction(rt).call(rt, std::move(errorObj));
                            }
                        });
                        {
                            std::lock_guard<std::mutex> lock(g_sseConnectionsMutex);
                            g_sseConnections.erase(connectionId);
                        }
                        return;
                    }

                    jsCallInvoker->invokeAsync([sharedOnOpen]() {
                        if (!g_jsRuntime) return;
                        jsi::Runtime& rt = *g_jsRuntime;
                        if (sharedOnOpen->isObject()) sharedOnOpen->asObject(rt).asFunction(rt).call(rt);
                    });

                    connection->stream = streamingResponse.getBody();
                    std::string buffer;

                    while (connection->active.load()) {
                        auto chunkOpt = connection->stream->read();
                        if (!chunkOpt.has_value()) break;

                        ByteBuffer& chunk = chunkOpt.value();
                        std::string chunkStr(reinterpret_cast<const char*>(chunk.data()), chunk.size());
                        buffer += chunkStr;

                        std::string eventType, eventId, eventData;
                        int retryMs;

                        while (parseSSEEvent(buffer, eventType, eventId, eventData, retryMs)) {
                            if (retryMs >= 0 && sharedOnRetry->isObject()) {
                                jsCallInvoker->invokeAsync([sharedOnRetry, retryMs]() {
                                    if (!g_jsRuntime) return;
                                    jsi::Runtime& rt = *g_jsRuntime;
                                    sharedOnRetry->asObject(rt).asFunction(rt).call(rt, retryMs);
                                });
                            }

                            if (eventData.empty()) continue;
                            std::string capturedType = eventType;
                            std::string capturedId = eventId;
                            std::string capturedData = eventData;

                            jsCallInvoker->invokeAsync([sharedOnMessage, capturedType, capturedId, capturedData]() {
                                if (!g_jsRuntime) return;
                                jsi::Runtime& rt = *g_jsRuntime;
                                if (sharedOnMessage->isObject()) {
                                    jsi::Object eventObj(rt);
                                    eventObj.setProperty(rt, "type", jsi::String::createFromUtf8(rt, capturedType));
                                    eventObj.setProperty(rt, "data", jsi::String::createFromUtf8(rt, capturedData));
                                    eventObj.setProperty(rt, "lastEventId", jsi::String::createFromUtf8(rt, capturedId));
                                    sharedOnMessage->asObject(rt).asFunction(rt).call(rt, std::move(eventObj));
                                }
                            });
                        }
                    }

                    bool shouldNotifyClose = connection->active.load();

                    {
                        std::lock_guard<std::mutex> lock(g_sseConnectionsMutex);
                        g_sseConnections.erase(connectionId);
                    }

                    if (shouldNotifyClose) {
                        std::string errorMsg = "SSE stream ended";
                        jsCallInvoker->invokeAsync([sharedOnError, errorMsg]() {
                            if (!g_jsRuntime) return;
                            jsi::Runtime& rt = *g_jsRuntime;
                            if (sharedOnError->isObject()) {
                                jsi::Object errorObj(rt);
                                errorObj.setProperty(rt, "message", jsi::String::createFromUtf8(rt, errorMsg));
                                sharedOnError->asObject(rt).asFunction(rt).call(rt, std::move(errorObj));
                            }
                        });
                    }

                } catch (const std::exception& e) {
                    if (connection->abortSignal->load() || !connection->active.load()) {
                        std::lock_guard<std::mutex> lock(g_sseConnectionsMutex);
                        g_sseConnections.erase(connectionId);
                        return;
                    }
                    std::string errorMsg = e.what();
                    jsCallInvoker->invokeAsync([sharedOnError, errorMsg]() {
                        if (!g_jsRuntime) return;
                        jsi::Runtime& rt = *g_jsRuntime;
                        if (sharedOnError->isObject()) {
                            jsi::Object errorObj(rt);
                            errorObj.setProperty(rt, "message", jsi::String::createFromUtf8(rt, errorMsg));
                            sharedOnError->asObject(rt).asFunction(rt).call(rt, std::move(errorObj));
                        }
                    });
                    {
                        std::lock_guard<std::mutex> lock(g_sseConnectionsMutex);
                        g_sseConnections.erase(connectionId);
                    }
                }
            }).detach();

            return jsi::Value(connectionId);
        });

    auto nativeSSEClose = jsi::Function::createFromHostFunction(
        jsiRuntime,
        jsi::PropNameID::forAscii(jsiRuntime, "nativeSSEClose"),
        1,
        [](jsi::Runtime &runtime, const jsi::Value &thisValue, const jsi::Value *arguments, size_t count) -> jsi::Value {
            if (count < 1 || !arguments[0].isNumber()) return jsi::Value(false);
            int connectionId = static_cast<int>(arguments[0].asNumber());

            std::lock_guard<std::mutex> lock(g_sseConnectionsMutex);
            auto it = g_sseConnections.find(connectionId);
            if (it != g_sseConnections.end()) {
                it->second->active.store(false);
                it->second->abortSignal->store(true);
                if (it->second->stream) it->second->stream->cancel();
                g_sseConnections.erase(it);
                return jsi::Value(true);
            }
            return jsi::Value(false);
        });

    auto getVersion = jsi::Function::createFromHostFunction(
        jsiRuntime,
        jsi::PropNameID::forAscii(jsiRuntime, "getVersion"),
        0,
        [](jsi::Runtime &runtime, const jsi::Value &thisValue, const jsi::Value *arguments, size_t count) -> jsi::Value {
            return jsi::String::createFromUtf8(runtime, "1.0.0");
        });

    browserNetworkModule.setProperty(jsiRuntime, "nativePerformRequest", std::move(nativePerformRequest));
    browserNetworkModule.setProperty(jsiRuntime, "nativePerformStreamRequest", std::move(nativePerformStreamRequest));
    browserNetworkModule.setProperty(jsiRuntime, "nativeAbortStreamRequest", std::move(nativeAbortStreamRequest));
    browserNetworkModule.setProperty(jsiRuntime, "nativeSSEConnect", std::move(nativeSSEConnect));
    browserNetworkModule.setProperty(jsiRuntime, "nativeSSEClose", std::move(nativeSSEClose));
    browserNetworkModule.setProperty(jsiRuntime, "getVersion", std::move(getVersion));

    jsiRuntime.global().setProperty(jsiRuntime, "__BrowserNetworkProxy", browserNetworkModule);
    jsiRuntime.global().setProperty(jsiRuntime, "browserNetwork", browserNetworkModule);

    installWebSocket(jsiRuntime, browserNetworkModule, jsCallInvoker);
    
    LOGD("BrowserNetwork JSI module installed successfully (Redirects & Base64 enabled).");
}

std::shared_ptr<CookieJar> getSharedCookieJar() {
    std::lock_guard<std::mutex> lock(g_stackMutex);
    return g_cookieJar;
}

std::optional<Origin> getEffectiveOrigin() {
    std::lock_guard<std::mutex> lock(g_stackMutex);
    return g_effectiveOrigin;
}

} // namespace browsernetwork
