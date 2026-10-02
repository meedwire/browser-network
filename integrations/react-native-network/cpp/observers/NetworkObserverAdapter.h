#pragma once

#include <ports/INetworkAdapter.h>
#include "INetworkObserver.h"
#include <memory>
#include <string>
#include <atomic>

namespace browser_network {

/**
 * @class NetworkObserverAdapter
 * @brief Decorador que notifica observadores sobre eventos de rede.
 */
class NetworkObserverAdapter : public INetworkAdapter {
public:
    NetworkObserverAdapter(
        std::shared_ptr<INetworkAdapter> inner,
        std::shared_ptr<INetworkObserver> observer
    );

    std::future<HttpResponse> send(const HttpRequest& request) override;
    std::future<StreamingResponse> sendStreaming(const HttpRequest& request) override;

private:
    std::shared_ptr<INetworkAdapter> inner_;
    std::shared_ptr<INetworkObserver> observer_;
    std::atomic<int> nextRequestId_{1};

    std::string generateRequestId();
    void reportRequest(const std::string& id, const HttpRequest& request);
    void reportResponse(const std::string& id, const HttpResponse& response);
    void reportStreamingResponseStart(const std::string& id, const StreamingResponse& response);
    void reportError(const std::string& id, const std::string& message);
};

} // namespace browser_network
