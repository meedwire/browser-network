#include "core/security/Origin.h"
#include <ReactCommon/CallInvoker.h>
#include <memory>
#include <optional>
#include <string>

namespace browser_network {
class CookieJar;
}

namespace browsernetwork {

using namespace facebook;

void install(facebook::jsi::Runtime &jsiRuntime,
             std::shared_ptr<facebook::react::CallInvoker> jsCallInvoker,
             const std::string& bundleId,
             const std::string& cookieStoragePath);

std::shared_ptr<browser_network::CookieJar> getSharedCookieJar();
std::optional<browser_network::Origin> getEffectiveOrigin();

} // namespace browsernetwork
