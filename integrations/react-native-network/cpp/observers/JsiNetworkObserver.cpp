#include "JsiNetworkObserver.h"

namespace browser_network {

JsiNetworkObserver::JsiNetworkObserver(
    jsi::Runtime& runtime,
    std::shared_ptr<react::CallInvoker> callInvoker,
    jsi::Function callback
) : runtime_(runtime), 
    callInvoker_(std::move(callInvoker)), 
    callback_(std::make_shared<jsi::Function>(std::move(callback))) {}

jsi::Object JsiNetworkObserver::headersToObject(jsi::Runtime& runtime, const HeaderMap& headers) {
    jsi::Object obj(runtime);
    for (const auto& [key, value] : headers) {
        obj.setProperty(runtime, key.c_str(), jsi::String::createFromUtf8(runtime, value));
    }
    return obj;
}

void JsiNetworkObserver::onBeforeRequest(const NetworkRequestEvent& event) {
    auto callback = callback_;
    callInvoker_->invokeAsync([this, event, callback]() {
        jsi::Object jsiEvent(runtime_);
        jsiEvent.setProperty(runtime_, "type", "beforeRequest");
        jsiEvent.setProperty(runtime_, "requestId", jsi::String::createFromUtf8(runtime_, event.requestId));
        jsiEvent.setProperty(runtime_, "url", jsi::String::createFromUtf8(runtime_, event.url));
        jsiEvent.setProperty(runtime_, "method", jsi::String::createFromUtf8(runtime_, event.method));
        jsiEvent.setProperty(runtime_, "headers", headersToObject(runtime_, event.headers));
        
        if (!event.body.empty()) {
            std::string body(event.body.begin(), event.body.end());
            jsiEvent.setProperty(runtime_, "body", jsi::String::createFromUtf8(runtime_, body));
        }

        callback->call(runtime_, std::move(jsiEvent));
    });
}

void JsiNetworkObserver::onResponseReceived(const NetworkResponseEvent& event) {
    auto callback = callback_;
    callInvoker_->invokeAsync([this, event, callback]() {
        jsi::Object jsiEvent(runtime_);
        jsiEvent.setProperty(runtime_, "type", "responseReceived");
        jsiEvent.setProperty(runtime_, "requestId", jsi::String::createFromUtf8(runtime_, event.requestId));
        jsiEvent.setProperty(runtime_, "status", jsi::Value(event.status));
        jsiEvent.setProperty(runtime_, "url", jsi::String::createFromUtf8(runtime_, event.url));
        jsiEvent.setProperty(runtime_, "headers", headersToObject(runtime_, event.headers));
        
        if (!event.body.empty()) {
            std::string body(event.body.begin(), event.body.end());
            jsiEvent.setProperty(runtime_, "body", jsi::String::createFromUtf8(runtime_, body));
        }

        callback->call(runtime_, std::move(jsiEvent));
    });
}

void JsiNetworkObserver::onError(const std::string& requestId, const std::string& message) {
    auto callback = callback_;
    callInvoker_->invokeAsync([this, requestId, message, callback]() {
        jsi::Object jsiEvent(runtime_);
        jsiEvent.setProperty(runtime_, "type", "error");
        jsiEvent.setProperty(runtime_, "requestId", jsi::String::createFromUtf8(runtime_, requestId));
        jsiEvent.setProperty(runtime_, "message", jsi::String::createFromUtf8(runtime_, message));

        callback->call(runtime_, std::move(jsiEvent));
    });
}

} // namespace browser_network
