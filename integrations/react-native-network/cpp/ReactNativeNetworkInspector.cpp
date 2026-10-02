#include "ReactNativeNetworkInspector.h"
#include <react/networking/NetworkReporter.h>
#include <fbjni/fbjni.h>

#ifdef __ANDROID__
#include <android/log.h>
#define INSPECTOR_LOG(...) __android_log_print(ANDROID_LOG_DEBUG, "NetworkInspector", __VA_ARGS__)
#else
#define INSPECTOR_LOG(...) printf(__VA_ARGS__)
#endif

namespace browser_network {

ReactNativeNetworkInspector::ReactNativeNetworkInspector() {}

bool ReactNativeNetworkInspector::isInspectorEnabled() const {
    try {
        facebook::jni::ThreadScope scope;
        return facebook::react::NetworkReporter::getInstance().isDebuggingEnabled();
    } catch (const std::exception& e) {
        INSPECTOR_LOG("isInspectorEnabled exception: %s", e.what());
        return false;
    } catch (...) {
        INSPECTOR_LOG("isInspectorEnabled unknown exception");
        return false;
    }
}

facebook::react::Headers ReactNativeNetworkInspector::convertHeaders(const std::map<std::string, std::string>& headers) {
    facebook::react::Headers result;
    for (const auto& [key, value] : headers) {
        result[key] = value;
    }
    return result;
}

void ReactNativeNetworkInspector::reportRequestStart(
    const std::string& requestId,
    const InspectorRequestInfo& info
) {
    try {
        facebook::jni::ThreadScope scope;
        facebook::react::RequestInfo rnInfo;
        rnInfo.url = info.url;
        rnInfo.httpMethod = info.method;
        rnInfo.headers = convertHeaders(info.headers);
        rnInfo.httpBody = info.body;

        facebook::react::NetworkReporter::getInstance().reportRequestStart(
            requestId,
            rnInfo,
            0,
            std::nullopt
        );
    } catch (const std::exception& e) {
        INSPECTOR_LOG("reportRequestStart exception: %s", e.what());
    } catch (...) {
        INSPECTOR_LOG("reportRequestStart unknown exception");
    }
}

void ReactNativeNetworkInspector::reportConnectionTiming(
    const std::string& requestId,
    const InspectorRequestInfo& info
) {
    try {
        facebook::jni::ThreadScope scope;
        auto headers = convertHeaders(info.headers);
        facebook::react::NetworkReporter::getInstance().reportConnectionTiming(
            requestId,
            headers
        );
    } catch (const std::exception& e) {
        INSPECTOR_LOG("reportConnectionTiming exception: %s", e.what());
    } catch (...) {
        INSPECTOR_LOG("reportConnectionTiming unknown exception");
    }
}

void ReactNativeNetworkInspector::reportResponseStart(
    const std::string& requestId,
    const InspectorResponseInfo& info
) {
    try {
        facebook::jni::ThreadScope scope;
        facebook::react::ResponseInfo rnInfo;
        rnInfo.url = info.url;
        rnInfo.statusCode = (uint16_t)info.status;
        rnInfo.headers = convertHeaders(info.headers);

        facebook::react::NetworkReporter::getInstance().reportResponseStart(
            requestId,
            rnInfo,
            0
        );
    } catch (const std::exception& e) {
        INSPECTOR_LOG("reportResponseStart exception: %s", e.what());
    } catch (...) {
        INSPECTOR_LOG("reportResponseStart unknown exception");
    }
}

void ReactNativeNetworkInspector::reportDataReceived(
    const std::string& requestId,
    int length
) {
    try {
        facebook::jni::ThreadScope scope;
        facebook::react::NetworkReporter::getInstance().reportDataReceived(
            requestId,
            length,
            std::nullopt
        );
    } catch (const std::exception& e) {
        INSPECTOR_LOG("reportDataReceived exception: %s", e.what());
    } catch (...) {
        INSPECTOR_LOG("reportDataReceived unknown exception");
    }
}

void ReactNativeNetworkInspector::storeResponseBody(
    const std::string& requestId,
    const std::string& body,
    bool base64Encoded
) {
    try {
        facebook::jni::ThreadScope scope;
        facebook::react::NetworkReporter::getInstance().storeResponseBody(
            requestId,
            body,
            base64Encoded
        );
    } catch (const std::exception& e) {
        INSPECTOR_LOG("storeResponseBody exception: %s", e.what());
    } catch (...) {
        INSPECTOR_LOG("storeResponseBody unknown exception");
    }
}

void ReactNativeNetworkInspector::reportResponseEnd(
    const std::string& requestId
) {
    try {
        facebook::jni::ThreadScope scope;
        facebook::react::NetworkReporter::getInstance().reportResponseEnd(
            requestId,
            0
        );
    } catch (const std::exception& e) {
        INSPECTOR_LOG("reportResponseEnd exception: %s", e.what());
    } catch (...) {
        INSPECTOR_LOG("reportResponseEnd unknown exception");
    }
}

void ReactNativeNetworkInspector::reportRequestFailed(
    const std::string& requestId,
    const std::string& error,
    bool cancelled
) {
    try {
        facebook::jni::ThreadScope scope;
        facebook::react::NetworkReporter::getInstance().reportRequestFailed(
            requestId,
            cancelled
        );
    } catch (const std::exception& e) {
        INSPECTOR_LOG("reportRequestFailed exception: %s", e.what());
    } catch (...) {
        INSPECTOR_LOG("reportRequestFailed unknown exception");
    }
}

} // namespace browser_network
