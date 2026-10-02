#pragma once

#include "INetworkObserver.h"
#include <jsi/jsi.h>
#include <ReactCommon/CallInvoker.h>
#include <memory>
#include <mutex>

namespace browser_network {

using namespace facebook;

/**
 * @class JsiNetworkObserver
 * @brief Reporta eventos de rede para um callback JavaScript.
 */
class JsiNetworkObserver : public INetworkObserver {
public:
    JsiNetworkObserver(
        jsi::Runtime& runtime,
        std::shared_ptr<react::CallInvoker> callInvoker,
        jsi::Function callback
    );

    bool isEnabled() const override { return true; }
    void onBeforeRequest(const NetworkRequestEvent& event) override;
    void onResponseReceived(const NetworkResponseEvent& event) override;
    void onError(const std::string& requestId, const std::string& message) override;

private:
    jsi::Runtime& runtime_;
    std::shared_ptr<react::CallInvoker> callInvoker_;
    std::shared_ptr<jsi::Function> callback_;
    std::mutex mutex_;

    jsi::Object headersToObject(jsi::Runtime& runtime, const HeaderMap& headers);
};

} // namespace browser_network
