#pragma once

#include <string>
#include <map>
#include <vector>

namespace browser_network {

struct InspectorRequestInfo {
    std::string url;
    std::string method;
    std::map<std::string, std::string> headers;
    std::string body;
};

struct InspectorResponseInfo {
    std::string url;
    int status;
    std::map<std::string, std::string> headers;
};

class INetworkInspector {
public:
    virtual ~INetworkInspector() = default;

    virtual bool isInspectorEnabled() const = 0;

    virtual void reportRequestStart(
        const std::string& requestId,
        const InspectorRequestInfo& info
    ) = 0;

    virtual void reportConnectionTiming(
        const std::string& requestId,
        const InspectorRequestInfo& info
    ) = 0;

    virtual void reportResponseStart(
        const std::string& requestId,
        const InspectorResponseInfo& info
    ) = 0;

    virtual void reportDataReceived(
        const std::string& requestId,
        int length
    ) = 0;

    virtual void storeResponseBody(
        const std::string& requestId,
        const std::string& body,
        bool base64Encoded
    ) = 0;

    virtual void reportResponseEnd(
        const std::string& requestId
    ) = 0;

    virtual void reportRequestFailed(
        const std::string& requestId,
        const std::string& error,
        bool cancelled
    ) = 0;
};

} // namespace browser_network
