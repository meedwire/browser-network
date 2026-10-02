#include "InspectorNetworkObserver.h"

namespace browser_network {

InspectorNetworkObserver::InspectorNetworkObserver(std::shared_ptr<INetworkInspector> inspector)
    : inspector_(std::move(inspector)) {}

bool InspectorNetworkObserver::isEnabled() const {
    return inspector_ && inspector_->isInspectorEnabled();
}

void InspectorNetworkObserver::onBeforeRequest(const NetworkRequestEvent& event) {
    if (!isEnabled()) return;

    InspectorRequestInfo info;
    info.url = event.url;
    info.method = event.method;
    // Conversão manual de headers (HeaderMap -> std::map)
    for (const auto& [key, value] : event.headers) {
        info.headers[key] = value;
    }
    info.body = std::string(event.body.begin(), event.body.end());

    inspector_->reportRequestStart(event.requestId, info);
    // Expo and RN send "ExtraInfo" (headers again) for connection timing.
    inspector_->reportConnectionTiming(event.requestId, info);
}

void InspectorNetworkObserver::onResponseReceived(const NetworkResponseEvent& event) {
    if (!isEnabled()) return;

    // 1. Response Start
    InspectorResponseInfo info;
    info.status = event.status;
    info.url = event.url;
    // Conversão manual de headers (HeaderMap -> std::map)
    for (const auto& [key, value] : event.headers) {
        info.headers[key] = value;
    }
    inspector_->reportResponseStart(event.requestId, info);

    // 2. Data Received (Full body)
    inspector_->reportDataReceived(event.requestId, (int)event.body.size());

    // 3. Store Body (if text/supported)
    // Assuming text for simplicity, or we can check headers.
    // Ideally we pass base64 if binary.
    std::string bodyStr(event.body.begin(), event.body.end());
    inspector_->storeResponseBody(event.requestId, bodyStr, false);

    // 4. End
    inspector_->reportResponseEnd(event.requestId);
}

void InspectorNetworkObserver::onError(const std::string& requestId, const std::string& message) {
    if (!isEnabled()) return;
    inspector_->reportRequestFailed(requestId, message, false);
}

void InspectorNetworkObserver::onStreamingResponseStart(const StreamingResponseStartEvent& event) {
    if (!isEnabled()) return;

    // 1. Response Start (headers recebidos)
    InspectorResponseInfo info;
    info.status = event.status;
    info.url = event.url;
    for (const auto& [key, value] : event.headers) {
        info.headers[key] = value;
    }
    inspector_->reportResponseStart(event.requestId, info);
}

void InspectorNetworkObserver::onStreamingDataReceived(const std::string& requestId, size_t length) {
    if (!isEnabled()) return;
    inspector_->reportDataReceived(requestId, static_cast<int>(length));
}

void InspectorNetworkObserver::onStreamingResponseEnd(const std::string& requestId, const std::string& body, bool base64Encoded) {
    if (!isEnabled()) return;

    // Armazenar o body para preview no DevTools
    if (!body.empty()) {
        inspector_->storeResponseBody(requestId, body, base64Encoded);
    }

    // Marcar a requisição como completa
    inspector_->reportResponseEnd(requestId);
}

} // namespace browser_network
