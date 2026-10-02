#pragma once

#include "../HttpRequest.h"
#include "Origin.h"
#include <chrono>
#include <cstddef>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace browser_network {

class ICookiePersistence;

using CookieTime = std::chrono::system_clock::time_point;

enum class CookieSameSiteContext {
    Strict,
    Lax,
    CrossSite,
};

struct CookieRequestOptions {
    CookieSameSiteContext sameSiteContext = CookieSameSiteContext::Strict;
    bool includeHttpOnly = true;
    CookieTime now = std::chrono::system_clock::now();
};

struct CookieRequestContext {
    std::string requestUrl;
    std::optional<Origin> siteForCookies;
    CredentialsMode credentialsMode = CredentialsMode::Include;
    HttpMethod method = HttpMethod::GET;
    bool isTopLevelNavigation = false;
};

struct StoredCookie {
    std::string name;
    std::string value;
    std::string domain;
    std::string path;
    std::string sameSite;
    bool secure = false;
    bool httpOnly = false;
    bool hostOnly = true;
    bool persistent = false;
    std::optional<CookieTime> expiresAt;
    CookieTime creationTime{};
    CookieTime lastAccessTime{};

    bool isExpired(CookieTime now) const {
        return expiresAt.has_value() && expiresAt.value() <= now;
    }
};

struct CookieSetResult {
    size_t accepted = 0;
    size_t rejected = 0;
};

class CookieJar {
public:
    explicit CookieJar(std::shared_ptr<ICookiePersistence> persistence = nullptr);

    bool setCookieFromHeader(const std::string& requestUrl,
                             const std::string& setCookieHeader,
                             CookieTime now = std::chrono::system_clock::now());

    CookieSetResult setCookiesFromHeaders(const std::string& requestUrl,
                                          const std::vector<std::string>& setCookieHeaders,
                                          CookieTime now = std::chrono::system_clock::now());

    std::vector<StoredCookie> getMatchingCookies(
        const std::string& requestUrl,
        const CookieRequestOptions& options = CookieRequestOptions()) const;

    std::string getCookieHeader(
        const std::string& requestUrl,
        const CookieRequestOptions& options = CookieRequestOptions()) const;

    void storeFromResponse(
        const std::string& responseUrl,
        const std::vector<std::string>& setCookieHeaders,
        CookieTime now = std::chrono::system_clock::now());

    std::string getCookieHeader(
        const CookieRequestContext& context,
        CookieTime now = std::chrono::system_clock::now()) const;

    std::vector<StoredCookie> getAllCookies(
        CookieTime now = std::chrono::system_clock::now()) const;

    std::vector<StoredCookie> snapshot(CookieTime now = std::chrono::system_clock::now()) const;

    size_t purgeExpired(CookieTime now = std::chrono::system_clock::now());

    size_t size(CookieTime now = std::chrono::system_clock::now()) const;

    void clearSessionCookies();
    void clearExpired(CookieTime now = std::chrono::system_clock::now());

private:
    void persistLocked(CookieTime now);

    mutable std::mutex mutex_;
    std::vector<StoredCookie> cookies_;
    std::shared_ptr<ICookiePersistence> persistence_;
};

} // namespace browser_network
