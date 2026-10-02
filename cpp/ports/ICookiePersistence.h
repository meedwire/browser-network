#pragma once

#include "../core/security/CookieJar.h"
#include <vector>

namespace browser_network {

class ICookiePersistence {
public:
    virtual ~ICookiePersistence() = default;

    virtual std::vector<StoredCookie> load() = 0;
    virtual bool save(const std::vector<StoredCookie>& cookies) = 0;
};

} // namespace browser_network
