#pragma once

#include "INetworkObserver.h"
#include "../inspectors/INetworkInspector.h"
#include <memory>

namespace browser_network {

/**
 * @class InspectorNetworkObserver
 * @brief Observador que delega eventos para um INetworkInspector (ex: DevTools).
 */
class InspectorNetworkObserver : public INetworkObserver {
public:
    explicit InspectorNetworkObserver(std::shared_ptr<INetworkInspector> inspector);

    bool isEnabled() const override;

    void onBeforeRequest(const NetworkRequestEvent& event) override;
    void onResponseReceived(const NetworkResponseEvent& event) override;
    void onError(const std::string& requestId, const std::string& message) override;

    // Streaming support
    void onStreamingResponseStart(const StreamingResponseStartEvent& event) override;
    void onStreamingDataReceived(const std::string& requestId, size_t length) override;
    void onStreamingResponseEnd(const std::string& requestId, const std::string& body, bool base64Encoded) override;

private:
    std::shared_ptr<INetworkInspector> inspector_;
};

} // namespace browser_network
