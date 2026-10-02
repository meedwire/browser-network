#include "NetworkObserverAdapter.h"
#include <core/RedirectTypes.h>
#include <future>
#include <chrono>

namespace browser_network {

NetworkObserverAdapter::NetworkObserverAdapter(
    std::shared_ptr<INetworkAdapter> inner,
    std::shared_ptr<INetworkObserver> observer
) : inner_(std::move(inner)), observer_(std::move(observer)) {}

std::string NetworkObserverAdapter::generateRequestId() {
    auto now = std::chrono::system_clock::now().time_since_epoch().count();
    return "browser-" + std::to_string(now) + "-" + std::to_string(nextRequestId_++);
}

void NetworkObserverAdapter::reportRequest(const std::string& id, const HttpRequest& request) {
    if (!observer_ || !observer_->isEnabled()) return;

    NetworkRequestEvent event;
    event.requestId = id;
    event.url = request.getUrl();
    event.method = httpMethodToString(request.getMethod());
    event.headers = request.getHeaders();
    event.body = request.getBody();

    observer_->onBeforeRequest(event);
}

void NetworkObserverAdapter::reportResponse(const std::string& id, const HttpResponse& response) {
    if (!observer_ || !observer_->isEnabled()) return;

    NetworkResponseEvent event;
    event.requestId = id;
    event.status = response.getStatus().code;
    event.url = response.getUrl();
    event.headers = response.getHeaders();
    event.body = response.getBody();

    observer_->onResponseReceived(event);
}

void NetworkObserverAdapter::reportStreamingResponseStart(const std::string& id, const StreamingResponse& response) {
    if (!observer_ || !observer_->isEnabled()) return;

    StreamingResponseStartEvent event;
    event.requestId = id;
    event.status = response.getStatus().code;
    event.url = response.getUrl();
    event.headers = response.getHeaders();

    observer_->onStreamingResponseStart(event);

    // Para streaming, reportamos o fim imediatamente após o início
    // Isso faz com que a requisição apareça como completa no DevTools
    // O body será vazio, mas pelo menos não fica "pending"
    observer_->onStreamingResponseEnd(id, "", false);
}

void NetworkObserverAdapter::reportError(const std::string& id, const std::string& message) {
    if (!observer_ || !observer_->isEnabled()) return;
    observer_->onError(id, message);
}

std::future<HttpResponse> NetworkObserverAdapter::send(const HttpRequest& request) {
    if (!observer_ || !observer_->isEnabled()) {
        return inner_->send(request);
    }

    std::string requestId = generateRequestId();
    reportRequest(requestId, request);

    return std::async(std::launch::async, [this, requestId, request]() mutable -> HttpResponse {
        try {
            HttpResponse response = inner_->send(request).get();

            // Report redirects: each hop gets its own request/response entry
            const auto& redirectInfo = response.getRedirectInfo();
            if (redirectInfo.redirected && !redirectInfo.chain.empty()) {
                // First redirect response (completes the initial request)
                NetworkResponseEvent firstResponse;
                firstResponse.requestId = requestId;
                firstResponse.status = redirectInfo.chain[0].statusCode;
                firstResponse.url = redirectInfo.chain[0].url;
                firstResponse.headers = redirectInfo.chain[0].responseHeaders;
                observer_->onResponseReceived(firstResponse);

                std::string prevRequestId = requestId;

                // Report each subsequent redirect hop
                for (size_t i = 1; i < redirectInfo.chain.size(); ++i) {
                    const auto& entry = redirectInfo.chain[i];

                    // Generate unique ID for this hop
                    std::string hopRequestId = requestId + "-hop" + std::to_string(i);

                    NetworkRequestEvent hopReq;
                    hopReq.requestId = hopRequestId;
                    hopReq.url = entry.url;
                    hopReq.method = entry.method;
                    hopReq.headers = entry.requestHeaders;
                    observer_->onBeforeRequest(hopReq);

                    NetworkResponseEvent hopRes;
                    hopRes.requestId = hopRequestId;
                    hopRes.status = entry.statusCode;
                    hopRes.url = entry.url;
                    hopRes.headers = entry.responseHeaders;
                    observer_->onResponseReceived(hopRes);
                }

                // Final request/response (destination after all redirects)
                std::string finalRequestId = requestId + "-final";

                NetworkRequestEvent finalReq;
                finalReq.requestId = finalRequestId;
                finalReq.url = response.getUrl();
                // Determine method after redirects
                const auto& lastEntry = redirectInfo.chain.back();
                std::string finalMethod = lastEntry.method;
                if (lastEntry.statusCode == 303) {
                    finalMethod = "GET";
                } else if ((lastEntry.statusCode == 301 || lastEntry.statusCode == 302) && lastEntry.method == "POST") {
                    finalMethod = "GET";
                }
                finalReq.method = finalMethod;
                finalReq.headers = request.getHeaders();
                observer_->onBeforeRequest(finalReq);

                NetworkResponseEvent finalRes;
                finalRes.requestId = finalRequestId;
                finalRes.status = response.getStatus().code;
                finalRes.url = response.getUrl();
                finalRes.headers = response.getHeaders();
                finalRes.body = response.getBody();
                observer_->onResponseReceived(finalRes);
            } else {
                // No redirects, report as normal
                reportResponse(requestId, response);
            }

            return response;
        } catch (const std::exception& e) {
            reportError(requestId, e.what());
            throw;
        }
    });
}

std::future<StreamingResponse> NetworkObserverAdapter::sendStreaming(const HttpRequest& request) {
    if (!observer_ || !observer_->isEnabled()) {
        return inner_->sendStreaming(request);
    }

    std::string requestId = generateRequestId();
    reportRequest(requestId, request);

    return std::async(std::launch::async, [this, requestId, request]() mutable -> StreamingResponse {
        try {
            StreamingResponse response = inner_->sendStreaming(request).get();

            // Reportar início e fim da resposta (headers recebidos)
            // Nota: Não usamos mais o ObservedReadableStream para evitar crashes
            // O body não será capturado, mas a requisição aparecerá como completa
            reportStreamingResponseStart(requestId, response);

            // Retornar a resposta original sem modificação
            return response;
        } catch (const std::exception& e) {
            reportError(requestId, e.what());
            throw;
        }
    });
}

} // namespace browser_network
