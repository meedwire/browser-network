#include <fbjni/fbjni.h>
#include <ReactCommon/CallInvokerHolder.h>
#include <jsi/jsi.h>
#include "BrowserNetworkJSI.h"

namespace browsernetwork {

using namespace facebook;

class BrowserNetworkModule : public jsi::HostObject {
    // Implementar se necessário usar como HostObject
};

class BrowserNetworkOnLoad : public jni::JavaClass<BrowserNetworkOnLoad> {
public:
    static auto constexpr kJavaDescriptor = "Lcom/network/NetworkModule;";

    static void registerNatives() {
        javaClassStatic()->registerNatives({
            makeNativeMethod("nativeInstall", BrowserNetworkOnLoad::nativeInstall),
        });
    }

private:
    static void nativeInstall(jni::alias_ref<jni::JavaClass<BrowserNetworkOnLoad>>, 
                             jlong runtimePointer,
                             jni::alias_ref<facebook::react::CallInvokerHolder::javaobject> jsCallInvokerHolder,
                             jni::alias_ref<jni::JString> bundleId,
                             jni::alias_ref<jni::JString> cookieStoragePath) {
        auto runtime = reinterpret_cast<jsi::Runtime *>(runtimePointer);
        if (runtime) {
            auto jsCallInvoker = jsCallInvokerHolder->cthis()->getCallInvoker();
            std::string bundleIdStr = bundleId->toStdString();
            std::string cookieStoragePathStr = cookieStoragePath->toStdString();
            browsernetwork::install(*runtime, jsCallInvoker, bundleIdStr, cookieStoragePathStr);
        }
    }
};

} // namespace browsernetwork

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *) {
    return facebook::jni::initialize(vm, [] {
        browsernetwork::BrowserNetworkOnLoad::registerNatives();
    });
}
