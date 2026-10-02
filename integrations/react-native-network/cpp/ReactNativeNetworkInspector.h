#pragma once

#include "inspectors/INetworkInspector.h"
#include <react/networking/NetworkReporter.h>
#include <memory>
#include <vector>

namespace browser_network {

/**
 * @class ReactNativeNetworkInspector
 * @brief Ponte entre INetworkInspector e o facebook::react::NetworkReporter do React Native
 */
class ReactNativeNetworkInspector : public INetworkInspector {
public:
    ReactNativeNetworkInspector();

    bool isInspectorEnabled() const override;

    void reportRequestStart(
        const std::string& requestId,
        const InspectorRequestInfo& info
    ) override;

    void reportConnectionTiming(
        const std::string& requestId,
        const InspectorRequestInfo& info
    ) override;

    void reportResponseStart(
        const std::string& requestId,
        const InspectorResponseInfo& info
    ) override;

    void reportDataReceived(
        const std::string& requestId,
        int length
    ) override;

    void storeResponseBody(
        const std::string& requestId,
        const std::string& body,
        bool base64Encoded
    ) override;

    void reportResponseEnd(
        const std::string& requestId
    ) override;

    void reportRequestFailed(
        const std::string& requestId,
        const std::string& error,
        bool cancelled
    ) override;

private:
   static facebook::react::Headers convertHeaders(const std::map<std::string, std::string>& headers);
};

} // namespace browser_network
