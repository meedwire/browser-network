#include "WebSocketJSI.h"
#include "BrowserNetworkJSI.h"
#include "adapters/websocket/WebSocketAdapter.h"

#include <jsi/jsi.h>
#include <map>
#include <mutex>
#include <atomic>
#include <thread>
#include <string>
#include <vector>
#include <cctype>

#ifdef __ANDROID__
#include <android/log.h>
#define LOG_TAG "BrowserNetworkWebSocket"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#else
#define LOGD(...) printf(__VA_ARGS__)
#define LOGE(...) fprintf(stderr, __VA_ARGS__)
#endif

namespace browsernetwork {

using namespace facebook;
using namespace browser_network;

static const std::string kBase64Chars =
             "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
             "abcdefghijklmnopqrstuvwxyz"
             "0123456789+/";

static inline bool is_base64(unsigned char c) {
    return (std::isalnum(c) || (c == '+') || (c == '/'));
}

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

            for (i = 0; i < 4; i++) {
                ret += kBase64Chars[char_array_4[i]];
            }
            i = 0;
        }
    }

    if (i) {
        for (j = i; j < 3; j++) {
            char_array_3[j] = '\0';
        }

        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        char_array_4[3] = char_array_3[2] & 0x3f;

        for (j = 0; j < i + 1; j++) {
            ret += kBase64Chars[char_array_4[j]];
        }

        while (i++ < 3) {
            ret += '=';
        }
    }

    return ret;
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
                char_array_4[i] = static_cast<unsigned char>(kBase64Chars.find(char_array_4[i]));
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
            char_array_4[j] = static_cast<unsigned char>(kBase64Chars.find(char_array_4[j]));
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

// Runtime global para acesso nos callbacks (assumindo single-instance)
static jsi::Runtime* g_wsJsRuntime = nullptr;

// Mapa para armazenar conexões WebSocket ativas
static std::mutex g_webSocketConnectionsMutex;
static std::map<int, std::shared_ptr<WebSocketAdapter>> g_webSocketConnections;
static std::atomic<int> g_webSocketConnectionIdCounter{0};

/**
 * @brief Instala os métodos do módulo WebSocket.
 */
void installWebSocket(jsi::Runtime &jsiRuntime, jsi::Object &browserNetworkModule, std::shared_ptr<react::CallInvoker> jsCallInvoker) {
    LOGD("Installing WebSocket JSI module...");
    g_wsJsRuntime = &jsiRuntime;

    /**
     * @brief Conecta a um endpoint WebSocket.
     *
     * @param url URL do servidor (ws:// ou wss://).
     * @param protocols Array de sub-protocolos (opcional).
     * @param options Objeto com callbacks (onOpen, onMessage, onError, onClose).
     * @return connectionId (number).
     */
    auto nativeWebSocketConnect = jsi::Function::createFromHostFunction(
        jsiRuntime,
        jsi::PropNameID::forAscii(jsiRuntime, "nativeWebSocketConnect"),
        3,
        [jsCallInvoker](jsi::Runtime &runtime, const jsi::Value &thisValue, const jsi::Value *arguments, size_t count) -> jsi::Value {
            if (count < 3 || !arguments[0].isString() || 
                !arguments[1].isObject() || !arguments[1].asObject(runtime).isArray(runtime) || 
                !arguments[2].isObject()) {
                LOGE("Invalid arguments to nativeWebSocketConnect");
                return jsi::Value(-1);
            }

            std::string url = arguments[0].asString(runtime).utf8(runtime);
            jsi::Array protocolsArray = arguments[1].asObject(runtime).asArray(runtime);
            jsi::Object options = arguments[2].asObject(runtime);

            // Extrair protocolos
            std::vector<std::string> protocols;
            size_t protocolsLength = protocolsArray.size(runtime);
            for (size_t i = 0; i < protocolsLength; i++) {
                jsi::Value protocolValue = protocolsArray.getValueAtIndex(runtime, i);
                if (protocolValue.isString()) {
                    protocols.push_back(protocolValue.asString(runtime).utf8(runtime));
                }
            }

            LOGD("WebSocket Connect URL: %s", url.c_str());

            // Capturar callbacks do JS
            // Usamos shared_ptr<jsi::Value> para manter referência segura aos objetos JS
            auto sharedOnOpen = std::make_shared<jsi::Value>(jsi::Value::undefined());
            auto sharedOnMessage = std::make_shared<jsi::Value>(jsi::Value::undefined());
            auto sharedOnError = std::make_shared<jsi::Value>(jsi::Value::undefined());
            auto sharedOnClose = std::make_shared<jsi::Value>(jsi::Value::undefined());
            
            if (options.hasProperty(runtime, "onOpen")) {
                jsi::Value val = options.getProperty(runtime, "onOpen");
                if (val.isObject() && val.asObject(runtime).isFunction(runtime)) {
                    *sharedOnOpen = jsi::Value(runtime, val);
                }
            }
            if (options.hasProperty(runtime, "onMessage")) {
                 jsi::Value val = options.getProperty(runtime, "onMessage");
                if (val.isObject() && val.asObject(runtime).isFunction(runtime)) {
                    *sharedOnMessage = jsi::Value(runtime, val);
                }
            }
            if (options.hasProperty(runtime, "onError")) {
                 jsi::Value val = options.getProperty(runtime, "onError");
                if (val.isObject() && val.asObject(runtime).isFunction(runtime)) {
                    *sharedOnError = jsi::Value(runtime, val);
                }
            }
            if (options.hasProperty(runtime, "onClose")) {
                 jsi::Value val = options.getProperty(runtime, "onClose");
                if (val.isObject() && val.asObject(runtime).isFunction(runtime)) {
                    *sharedOnClose = jsi::Value(runtime, val);
                }
            }

            // Criar e configurar o adapter
            auto adapter =
                std::make_shared<WebSocketAdapter>(
                    url, protocols, getSharedCookieJar(), getEffectiveOrigin());
            int connectionId = g_webSocketConnectionIdCounter.fetch_add(1);

            // 1. Callback OnOpen
            std::weak_ptr<WebSocketAdapter> weakAdapter = adapter;
            adapter->setOnOpen([jsCallInvoker, sharedOnOpen, connectionId, weakAdapter]() {
                jsCallInvoker->invokeAsync([sharedOnOpen, connectionId, weakAdapter]() {
                    if (!g_wsJsRuntime) return;
                    jsi::Runtime& rt = *g_wsJsRuntime;
                    auto adapter = weakAdapter.lock();
                    if (!adapter) return;
                    
                    if (sharedOnOpen->isObject() && sharedOnOpen->asObject(rt).isFunction(rt)) {
                        LOGD("Invoking WebSocket %d onOpen", connectionId);
                        jsi::Object event(rt);
                        event.setProperty(
                            rt,
                            "protocol",
                            jsi::String::createFromUtf8(rt, adapter->getProtocol()));
                        sharedOnOpen->asObject(rt).asFunction(rt).call(rt, std::move(event));
                    }
                });
            });

            // 2. Callback OnMessage
            adapter->setOnMessage([jsCallInvoker, sharedOnMessage, connectionId](const WebSocketMessage& msg) {
                // Copiar dados para o lambda
                std::string dataCopy;
                bool isBinary = false;
                std::string encoding = "utf8";

                if (std::holds_alternative<std::string>(msg.data)) {
                    dataCopy = std::get<std::string>(msg.data);
                } else {
                    const auto& bytes = std::get<ByteBuffer>(msg.data);
                    dataCopy = base64_encode(bytes.data(), static_cast<unsigned int>(bytes.size()));
                    isBinary = true;
                    encoding = "base64";
                }

                jsCallInvoker->invokeAsync([sharedOnMessage, dataCopy, isBinary, encoding, connectionId]() {
                    if (!g_wsJsRuntime) return;
                    jsi::Runtime& rt = *g_wsJsRuntime;

                    if (sharedOnMessage->isObject() && sharedOnMessage->asObject(rt).isFunction(rt)) {
                        jsi::Object event(rt);
                        event.setProperty(rt, "data", jsi::String::createFromUtf8(rt, dataCopy));
                        event.setProperty(rt, "type", jsi::String::createFromUtf8(rt, isBinary ? "binary" : "text"));
                        event.setProperty(rt, "encoding", jsi::String::createFromUtf8(rt, encoding));
                        
                        sharedOnMessage->asObject(rt).asFunction(rt).call(rt, std::move(event));
                    }
                });
            });

            // 3. Callback OnError
            adapter->setOnError([jsCallInvoker, sharedOnError, connectionId](const WebSocketError& error) {
                std::string errorMsg = error.message;
                jsCallInvoker->invokeAsync([sharedOnError, errorMsg, connectionId]() {
                    if (!g_wsJsRuntime) return;
                    jsi::Runtime& rt = *g_wsJsRuntime;

                    if (sharedOnError->isObject() && sharedOnError->asObject(rt).isFunction(rt)) {
                        LOGE("Invoking WebSocket %d onError: %s", connectionId, errorMsg.c_str());
                        jsi::Object event(rt);
                        event.setProperty(rt, "message", jsi::String::createFromUtf8(rt, errorMsg));
                        sharedOnError->asObject(rt).asFunction(rt).call(rt, std::move(event));
                    }
                });
            });

            // 4. Callback OnClose
            adapter->setOnClose([jsCallInvoker, sharedOnClose, connectionId](const WebSocketCloseInfo& info) {
                int code = info.code;
                std::string reason = info.reason;
                bool wasClean = info.wasClean;
                
                jsCallInvoker->invokeAsync([sharedOnClose, code, reason, wasClean, connectionId]() {
                    if (!g_wsJsRuntime) return;
                    jsi::Runtime& rt = *g_wsJsRuntime;

                    if (sharedOnClose->isObject() && sharedOnClose->asObject(rt).isFunction(rt)) {
                        LOGD("Invoking WebSocket %d onClose: %d %s", connectionId, code, reason.c_str());
                        jsi::Object event(rt);
                        event.setProperty(rt, "code", code);
                        event.setProperty(rt, "reason", jsi::String::createFromUtf8(rt, reason));
                        event.setProperty(rt, "wasClean", wasClean);
                        sharedOnClose->asObject(rt).asFunction(rt).call(rt, std::move(event));
                    }
                    
                    // Remover conexão do mapa ao fechar
                    {
                        std::lock_guard<std::mutex> lock(g_webSocketConnectionsMutex);
                        g_webSocketConnections.erase(connectionId);
                    }
                });
            });

            // Armazenar conexão
            {
                std::lock_guard<std::mutex> lock(g_webSocketConnectionsMutex);
                g_webSocketConnections[connectionId] = adapter;
            }

            // Conectar (assíncrono internamente no adapter ou thread dedicada)
            // O WebSocketAdapter usa IXWebSocket que gerencia suas threads, 
            // mas o método connect() pode bloquear até iniciar handshake.
            // Para não bloquear a JS thread, rodamos em std::thread separada.
             std::thread([adapter]() {
                adapter->connect();
            }).detach();

            return jsi::Value(connectionId);
        });
    browserNetworkModule.setProperty(jsiRuntime, "nativeWebSocketConnect", std::move(nativeWebSocketConnect));

    /**
     * @brief Envia dados pelo WebSocket.
     *
     * @param connectionId ID da conexão.
     * @param data Dados (string). Futuramente ArrayBuffer.
     */
    auto nativeWebSocketSend = jsi::Function::createFromHostFunction(
        jsiRuntime,
        jsi::PropNameID::forAscii(jsiRuntime, "nativeWebSocketSend"),
        3,
        [](jsi::Runtime &runtime, const jsi::Value &thisValue, const jsi::Value *arguments, size_t count) -> jsi::Value {
            if (count < 2 || !arguments[0].isNumber() || !arguments[1].isString()) {
                return jsi::Value::undefined();
            }

            int connectionId = static_cast<int>(arguments[0].asNumber());
            std::string data = arguments[1].asString(runtime).utf8(runtime);
            bool isBinary = false;
            bool isBase64 = false;

            if (count > 2 && arguments[2].isObject()) {
                jsi::Object options = arguments[2].asObject(runtime);
                if (options.hasProperty(runtime, "binary")) {
                    jsi::Value value = options.getProperty(runtime, "binary");
                    if (value.isBool()) {
                        isBinary = value.getBool();
                    }
                }
                if (options.hasProperty(runtime, "base64")) {
                    jsi::Value value = options.getProperty(runtime, "base64");
                    if (value.isBool()) {
                        isBase64 = value.getBool();
                    }
                }
            }

            std::shared_ptr<WebSocketAdapter> adapter;
            {
                std::lock_guard<std::mutex> lock(g_webSocketConnectionsMutex);
                auto it = g_webSocketConnections.find(connectionId);
                if (it != g_webSocketConnections.end()) {
                    adapter = it->second;
                }
            }

            if (adapter) {
                if (isBinary) {
                    if (isBase64) {
                        adapter->send(base64_decode(data));
                    } else {
                        ByteBuffer bytes(data.begin(), data.end());
                        adapter->send(bytes);
                    }
                } else {
                    adapter->send(data);
                }
            }

            return jsi::Value::undefined();
        });
    browserNetworkModule.setProperty(jsiRuntime, "nativeWebSocketSend", std::move(nativeWebSocketSend));

    /**
     * @brief Fecha a conexão WebSocket.
     *
     * @param connectionId ID da conexão.
     * @param code Código de status (opcional, default 1000).
     * @param reason Razão (opcional).
     */
    auto nativeWebSocketClose = jsi::Function::createFromHostFunction(
        jsiRuntime,
        jsi::PropNameID::forAscii(jsiRuntime, "nativeWebSocketClose"),
        3,
        [](jsi::Runtime &runtime, const jsi::Value &thisValue, const jsi::Value *arguments, size_t count) -> jsi::Value {
            if (count < 1 || !arguments[0].isNumber()) {
                return jsi::Value::undefined();
            }

            int connectionId = static_cast<int>(arguments[0].asNumber());
            int code = 1000;
            std::string reason = "";

            if (count > 1 && arguments[1].isNumber()) {
                code = static_cast<int>(arguments[1].asNumber());
            }
            if (count > 2 && arguments[2].isString()) {
                reason = arguments[2].asString(runtime).utf8(runtime);
            }

            std::shared_ptr<WebSocketAdapter> adapter;
            {
                std::lock_guard<std::mutex> lock(g_webSocketConnectionsMutex);
                auto it = g_webSocketConnections.find(connectionId);
                if (it != g_webSocketConnections.end()) {
                    adapter = it->second;
                }
            }

            if (adapter) {
                adapter->close(code, reason);
            }

            return jsi::Value::undefined();
        });
    browserNetworkModule.setProperty(jsiRuntime, "nativeWebSocketClose", std::move(nativeWebSocketClose));
}

} // namespace browsernetwork
