#include "WebSocketAdapter.h"
#include "../../core/security/CookieJar.h"
#include <string_view>
#include <utility>

namespace browser_network {

namespace {

std::string toHttpCookieUrl(const std::string& url) {
    constexpr std::string_view wsPrefix = "ws://";
    constexpr std::string_view wssPrefix = "wss://";

    if (url.rfind(wsPrefix.data(), 0) == 0) {
        return "http://" + url.substr(wsPrefix.size());
    }

    if (url.rfind(wssPrefix.data(), 0) == 0) {
        return "https://" + url.substr(wssPrefix.size());
    }

    return url;
}

std::string buildHandshakeCookieHeader(const std::shared_ptr<CookieJar>& cookieJar,
                                       const std::string& url,
                                       const std::optional<Origin>& siteForCookies) {
    if (!cookieJar) {
        return "";
    }

    CookieRequestContext context;
    context.requestUrl = toHttpCookieUrl(url);
    context.siteForCookies = siteForCookies;
    context.credentialsMode = CredentialsMode::Include;
    context.method = HttpMethod::GET;
    context.isTopLevelNavigation = false;
    return cookieJar->getCookieHeader(context);
}

} // namespace

WebSocketAdapter::WebSocketAdapter(const std::string& url,
                                   const std::vector<std::string>& protocols,
                                   std::shared_ptr<CookieJar> cookieJar,
                                   std::optional<Origin> siteForCookies)
    : url_(url),
      cookieJar_(std::move(cookieJar)),
      siteForCookies_(std::move(siteForCookies)) {
    ws_.setUrl(url);

    if (!protocols.empty()) {
        std::string protocolHeader;
        for (size_t i = 0; i < protocols.size(); ++i) {
            if (i > 0) {
                protocolHeader += ", ";
            }
            protocolHeader += protocols[i];
        }
        baseHeaders_["Sec-WebSocket-Protocol"] = protocolHeader;
    }

    ws_.setExtraHeaders(baseHeaders_);
    ws_.disableAutomaticReconnection();
    ws_.setPingInterval(30);
    setupEventHandlers();
}

WebSocketAdapter::~WebSocketAdapter() {
    ws_.stop();
}

void WebSocketAdapter::setOnOpen(OnOpenCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    onOpen_ = std::move(callback);
}

void WebSocketAdapter::setOnMessage(OnMessageCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    onMessage_ = std::move(callback);
}

void WebSocketAdapter::setOnError(OnErrorCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    onError_ = std::move(callback);
}

void WebSocketAdapter::setOnClose(OnCloseCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    onClose_ = std::move(callback);
}

void WebSocketAdapter::connect() {
    applyHandshakeHeaders();
    ws_.start();
}

void WebSocketAdapter::send(const std::string& text) {
    ws_.send(text);
}

void WebSocketAdapter::send(const ByteBuffer& data) {
    std::string binaryStr(data.begin(), data.end());
    ws_.sendBinary(binaryStr);
}

void WebSocketAdapter::close(int code, const std::string& reason) {
    ws_.close(code, reason);
}

WebSocketState WebSocketAdapter::getState() const {
    auto state = ws_.getReadyState();
    switch (state) {
        case ix::ReadyState::Connecting: return WebSocketState::Connecting;
        case ix::ReadyState::Open:       return WebSocketState::Open;
        case ix::ReadyState::Closing:    return WebSocketState::Closing;
        case ix::ReadyState::Closed:     return WebSocketState::Closed;
        default:                         return WebSocketState::Closed;
    }
}

const std::string& WebSocketAdapter::getUrl() const {
    return url_;
}

const std::string& WebSocketAdapter::getProtocol() const {
    return protocol_;
}

void WebSocketAdapter::setupEventHandlers() {
    ws_.setOnMessageCallback([this](const ix::WebSocketMessagePtr& msg) {
        std::lock_guard<std::mutex> lock(callbackMutex_);

        switch (msg->type) {
            case ix::WebSocketMessageType::Open: {
                protocol_ = msg->openInfo.protocol;
                if (protocol_.empty()) {
                    auto it = msg->openInfo.headers.find("Sec-WebSocket-Protocol");
                    if (it != msg->openInfo.headers.end()) {
                        protocol_ = it->second;
                    }
                }
                storeHandshakeCookies(msg->openInfo.headers);
                if (onOpen_) {
                    onOpen_();
                }
                break;
            }
            case ix::WebSocketMessageType::Message: {
                if (onMessage_) {
                    WebSocketMessage wsMsg;
                    wsMsg.isBinary = msg->binary;

                    if (msg->binary) {
                        ByteBuffer buffer(msg->str.begin(), msg->str.end());
                        wsMsg.data = std::move(buffer);
                    } else {
                        wsMsg.data = msg->str;
                    }

                    onMessage_(wsMsg);
                }
                break;
            }
            case ix::WebSocketMessageType::Error: {
                if (onError_) {
                    WebSocketError err;
                    err.message = msg->errorInfo.reason;
                    err.code = static_cast<int>(msg->errorInfo.http_status);
                    onError_(err);
                }
                break;
            }
            case ix::WebSocketMessageType::Close: {
                if (onClose_) {
                    WebSocketCloseInfo info;
                    info.code = msg->closeInfo.code;
                    info.reason = msg->closeInfo.reason;
                    info.wasClean = (msg->closeInfo.code == 1000);
                    onClose_(info);
                }
                break;
            }
            case ix::WebSocketMessageType::Ping:
            case ix::WebSocketMessageType::Pong:
            case ix::WebSocketMessageType::Fragment:
                break;
        }
    });
}

void WebSocketAdapter::applyHandshakeHeaders() {
    auto headers = baseHeaders_;
    const auto cookieHeader = buildHandshakeCookieHeader(cookieJar_, url_, siteForCookies_);
    if (!cookieHeader.empty() && headers.find("Cookie") == headers.end()) {
        headers["Cookie"] = cookieHeader;
    }
    if (siteForCookies_.has_value() && headers.find("Origin") == headers.end()) {
        headers["Origin"] = siteForCookies_->serialize();
    }

    ws_.setExtraHeaders(headers);
}

void WebSocketAdapter::storeHandshakeCookies(const ix::WebSocketHttpHeaders& headers) {
    if (!cookieJar_) {
        return;
    }

    auto it = headers.find("Set-Cookie");
    if (it == headers.end() || it->second.empty()) {
        return;
    }

    std::vector<std::string> values;
    size_t start = 0;
    while (start <= it->second.size()) {
        size_t end = it->second.find('\n', start);
        std::string value = it->second.substr(
            start,
            end == std::string::npos ? std::string::npos : end - start);
        while (!value.empty() && value.back() == '\r') {
            value.pop_back();
        }
        if (!value.empty()) {
            values.push_back(std::move(value));
        }
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }

    if (!values.empty()) {
        cookieJar_->storeFromResponse(toHttpCookieUrl(url_), values);
    }
}

} // namespace browser_network
