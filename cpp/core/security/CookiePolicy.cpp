#include "CookiePolicy.h"

#include "CookieJar.h"
#include "PublicSuffixList.h"
#include "../../ports/ICookiePersistence.h"

#include <algorithm>
#include <cctype>
#include <ctime>
#include <iomanip>
#include <limits>
#include <sstream>

namespace browser_network {
namespace {

struct ParsedUrl {
    Origin origin;
    std::string scheme;
    std::string host;
    std::string path;
    bool valid = false;
};

struct ApplyCookieResult {
    bool accepted = false;
    bool changed = false;
};

std::string trim(const std::string& value) {
    size_t start = value.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }

    size_t end = value.find_last_not_of(" \t\r\n");
    return value.substr(start, end - start + 1);
}

std::string toLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

bool startsWith(const std::string& value, const std::string& prefix) {
    return value.rfind(prefix, 0) == 0;
}

std::string normalizeDomain(const std::string& rawDomain) {
    std::string domain = toLower(trim(rawDomain));

    while (!domain.empty() && domain.front() == '.') {
        domain.erase(domain.begin());
    }

    while (!domain.empty() && domain.back() == '.') {
        domain.pop_back();
    }

    return domain;
}

bool isIpv4Address(const std::string& host) {
    if (host.empty()) {
        return false;
    }

    int dots = 0;
    for (char c : host) {
        if (c == '.') {
            ++dots;
            continue;
        }

        if (!std::isdigit(static_cast<unsigned char>(c))) {
            return false;
        }
    }

    return dots == 3;
}

bool isIpAddress(const std::string& host) {
    return isIpv4Address(host) || host.find(':') != std::string::npos;
}

bool domainMatches(const std::string& host, const std::string& domain) {
    if (host == domain) {
        return true;
    }

    if (host.size() <= domain.size()) {
        return false;
    }

    if (host.compare(host.size() - domain.size(), domain.size(), domain) != 0) {
        return false;
    }

    return host[host.size() - domain.size() - 1] == '.';
}

std::string defaultPath(const std::string& requestPath) {
    if (requestPath.empty() || requestPath.front() != '/') {
        return "/";
    }

    if (requestPath == "/") {
        return "/";
    }

    size_t lastSlash = requestPath.rfind('/');
    if (lastSlash == 0 || lastSlash == std::string::npos) {
        return "/";
    }

    return requestPath.substr(0, lastSlash);
}

bool pathMatches(const std::string& requestPath, const std::string& cookiePath) {
    if (requestPath == cookiePath) {
        return true;
    }

    if (requestPath.rfind(cookiePath, 0) != 0) {
        return false;
    }

    if (!cookiePath.empty() && cookiePath.back() == '/') {
        return true;
    }

    return requestPath.size() > cookiePath.size() &&
           requestPath[cookiePath.size()] == '/';
}

CookieTime fromUnixSeconds(int64_t unixSeconds) {
    return CookieTime{std::chrono::seconds(unixSeconds)};
}

int64_t toUnixSeconds(CookieTime time) {
    return std::chrono::duration_cast<std::chrono::seconds>(
               time.time_since_epoch())
        .count();
}

std::optional<int64_t> parseCookieDate(const std::string& rawDate) {
    std::tm tm = {};
    std::istringstream stream(rawDate);
    stream >> std::get_time(&tm, "%a, %d %b %Y %H:%M:%S GMT");
    if (stream.fail()) {
        return std::nullopt;
    }

#if defined(_WIN32)
    std::time_t timestamp = _mkgmtime(&tm);
#else
    std::time_t timestamp = timegm(&tm);
#endif
    if (timestamp == static_cast<std::time_t>(-1)) {
        return std::nullopt;
    }

    return static_cast<int64_t>(timestamp);
}

ParsedUrl parseUrl(const std::string& url) {
    ParsedUrl parsed;
    auto origin = Origin::fromUrl(url);
    if (!origin.has_value()) {
        return parsed;
    }

    parsed.origin = origin.value();
    parsed.origin.scheme = toLower(parsed.origin.scheme);
    parsed.origin.host = toLower(parsed.origin.host);
    parsed.scheme = parsed.origin.scheme;
    parsed.host = parsed.origin.host;

    size_t schemeEnd = url.find("://");
    size_t pathStart = schemeEnd == std::string::npos
        ? std::string::npos
        : url.find('/', schemeEnd + 3);
    if (pathStart == std::string::npos) {
        parsed.path = "/";
    } else {
        size_t pathEnd = url.find_first_of("?#", pathStart);
        parsed.path = url.substr(
            pathStart,
            pathEnd == std::string::npos ? std::string::npos : pathEnd - pathStart);
        if (parsed.path.empty()) {
            parsed.path = "/";
        }
    }

    parsed.valid = true;
    return parsed;
}

std::vector<std::string> splitLabels(const std::string& host) {
    std::vector<std::string> labels;
    std::stringstream stream(host);
    std::string label;
    while (std::getline(stream, label, '.')) {
        if (!label.empty()) {
            labels.push_back(label);
        }
    }
    return labels;
}

std::string registrableDomain(const std::string& host) {
    std::string lower = toLower(host);
    if (lower.empty() || lower == "localhost" || isIpAddress(lower)) {
        return lower;
    }

    const std::vector<std::string> labels = splitLabels(lower);
    if (labels.size() <= 1) {
        return lower;
    }

    const std::string lastTwo = labels[labels.size() - 2] + "." + labels.back();
    if (cookie_psl::isPublicSuffix(lastTwo) && labels.size() >= 3) {
        return labels[labels.size() - 3] + "." + lastTwo;
    }

    return lastTwo;
}

bool isSameSiteRequest(const std::optional<Origin>& siteForCookies,
                       const ParsedUrl& requestUrl) {
    if (!siteForCookies.has_value()) {
        return true;
    }

    return toLower(siteForCookies->scheme) == requestUrl.scheme &&
           registrableDomain(siteForCookies->host) == registrableDomain(requestUrl.host);
}

bool isSecureRequest(const ParsedUrl& parsedUrl) {
    return SecurityPolicy::isSecureContext(parsedUrl.origin);
}

std::string effectiveSameSite(const std::string& rawSameSite) {
    const std::string lower = toLower(rawSameSite);
    if (lower == "strict") {
        return "Strict";
    }
    if (lower == "none") {
        return "None";
    }
    return "Lax";
}

bool sameSiteAllowsSimpleRequest(const StoredCookie& cookie,
                                 CookieSameSiteContext context) {
    const std::string sameSite = effectiveSameSite(cookie.sameSite);
    if (sameSite == "None") {
        return true;
    }
    if (sameSite == "Strict") {
        return context == CookieSameSiteContext::Strict;
    }
    return context != CookieSameSiteContext::CrossSite;
}

bool sameSiteAllowsRequest(const StoredCookie& cookie,
                           const CookieRequestContext& context,
                           const ParsedUrl& requestUrl) {
    const std::string sameSite = effectiveSameSite(cookie.sameSite);
    const bool sameSiteRequest = isSameSiteRequest(context.siteForCookies, requestUrl);

    if (sameSite == "None") {
        return true;
    }

    if (sameSite == "Strict") {
        return sameSiteRequest;
    }

    if (sameSiteRequest) {
        return true;
    }

    return context.isTopLevelNavigation &&
           (context.method == HttpMethod::GET || context.method == HttpMethod::HEAD);
}

bool cookieExpired(const StoredCookie& cookie, CookieTime now) {
    return cookie.isExpired(now);
}

CookieTime maxCookieTime() {
    return CookieTime::max() - std::chrono::seconds(1);
}

std::optional<CookieTime> computeExpiry(const CookieAttributes& attrs, CookieTime now) {
    if (attrs.maxAgeSeconds.has_value()) {
        if (attrs.maxAgeSeconds.value() <= 0) {
            return now - std::chrono::seconds(1);
        }

        const int64_t remaining = attrs.maxAgeSeconds.value();
        const int64_t nowUnix = toUnixSeconds(now);
        if (remaining > (std::numeric_limits<int64_t>::max() - nowUnix)) {
            return maxCookieTime();
        }

        return now + std::chrono::seconds(remaining);
    }

    if (attrs.expiresUnixSeconds.has_value()) {
        return fromUnixSeconds(attrs.expiresUnixSeconds.value());
    }

    return std::nullopt;
}

ApplyCookieResult applySetCookie(std::vector<StoredCookie>& cookies,
                                 const ParsedUrl& responseUrl,
                                 const std::string& header,
                                 CookieTime now) {
    auto parsed = CookiePolicy::parseSetCookie(header);
    if (!parsed.has_value()) {
        return {};
    }

    CookieAttributes attrs = parsed.value();
    if (!CookiePolicy::shouldAcceptCookie(attrs, responseUrl.origin)) {
        return {};
    }

    StoredCookie cookie;
    cookie.name = attrs.name;
    cookie.value = attrs.value;
    cookie.secure = attrs.secure;
    cookie.httpOnly = attrs.httpOnly;
    cookie.sameSite = attrs.sameSite.empty() ? "Lax" : attrs.sameSite;
    cookie.creationTime = now;
    cookie.lastAccessTime = now;
    cookie.hostOnly = !attrs.domainSpecified;

    if (attrs.domainSpecified) {
        cookie.domain = normalizeDomain(attrs.domain);
    } else {
        cookie.domain = responseUrl.host;
    }

    if (attrs.pathSpecified && !attrs.path.empty() && attrs.path.front() == '/') {
        cookie.path = attrs.path;
    } else {
        cookie.path = defaultPath(responseUrl.path);
    }

    cookie.expiresAt = computeExpiry(attrs, now);
    cookie.persistent = cookie.expiresAt.has_value();

    const size_t before = cookies.size();
    cookies.erase(
        std::remove_if(cookies.begin(),
                       cookies.end(),
                       [&cookie](const StoredCookie& existing) {
                           return existing.name == cookie.name &&
                                  existing.domain == cookie.domain &&
                                  existing.path == cookie.path;
                       }),
        cookies.end());
    bool changed = cookies.size() != before;

    if (cookieExpired(cookie, now)) {
        return {true, changed};
    }

    cookies.push_back(std::move(cookie));
    return {true, true};
}

void sortCookies(std::vector<StoredCookie>& cookies) {
    std::stable_sort(cookies.begin(), cookies.end(), [](const StoredCookie& left,
                                                        const StoredCookie& right) {
        if (left.path.size() != right.path.size()) {
            return left.path.size() > right.path.size();
        }
        return left.creationTime < right.creationTime;
    });
}

} // namespace

std::optional<CookieAttributes> CookiePolicy::parseSetCookie(
    const std::string& setCookieHeader) {
    if (setCookieHeader.empty()) {
        return std::nullopt;
    }

    CookieAttributes cookie;
    std::istringstream stream(setCookieHeader);
    std::string part;
    std::vector<std::string> parts;

    while (std::getline(stream, part, ';')) {
        parts.push_back(trim(part));
    }

    if (parts.empty()) {
        return std::nullopt;
    }

    size_t eqPos = parts[0].find('=');
    if (eqPos == std::string::npos) {
        return std::nullopt;
    }

    cookie.name = trim(parts[0].substr(0, eqPos));
    cookie.value = trim(parts[0].substr(eqPos + 1));
    if (cookie.name.empty()) {
        return std::nullopt;
    }

    for (size_t i = 1; i < parts.size(); ++i) {
        const std::string& attribute = parts[i];
        if (attribute.empty()) {
            continue;
        }

        size_t attrEq = attribute.find('=');
        std::string attrName;
        std::string attrValue;
        if (attrEq == std::string::npos) {
            attrName = toLower(attribute);
        } else {
            attrName = toLower(trim(attribute.substr(0, attrEq)));
            attrValue = trim(attribute.substr(attrEq + 1));
        }

        if (attrName == "secure") {
            cookie.secure = true;
        } else if (attrName == "httponly") {
            cookie.httpOnly = true;
        } else if (attrName == "samesite") {
            const std::string lower = toLower(attrValue);
            if (lower == "strict") {
                cookie.sameSite = "Strict";
            } else if (lower == "lax") {
                cookie.sameSite = "Lax";
            } else if (lower == "none") {
                cookie.sameSite = "None";
            } else {
                cookie.sameSite = attrValue;
            }
        } else if (attrName == "domain") {
            cookie.domainSpecified = true;
            cookie.domain = normalizeDomain(attrValue);
        } else if (attrName == "path") {
            cookie.pathSpecified = true;
            cookie.path = attrValue;
        } else if (attrName == "max-age") {
            try {
                cookie.maxAgeSeconds = std::stoll(attrValue);
            } catch (...) {
                cookie.maxAgeSeconds.reset();
            }
        } else if (attrName == "expires") {
            cookie.expiresUnixSeconds = parseCookieDate(attrValue);
        }
    }

    return cookie;
}

bool CookiePolicy::shouldAcceptCookie(const CookieAttributes& cookie,
                                      const Origin& requestOrigin) {
    Origin normalizedOrigin = requestOrigin;
    normalizedOrigin.scheme = toLower(normalizedOrigin.scheme);
    normalizedOrigin.host = toLower(normalizedOrigin.host);
    const bool secureContext = SecurityPolicy::isSecureContext(normalizedOrigin);

    if (cookie.secure && !secureContext) {
        return false;
    }

    if (cookie.sameSite == "None" && (!cookie.secure || !secureContext)) {
        return false;
    }

    if (startsWith(cookie.name, "__Secure-") &&
        (!cookie.secure || !secureContext)) {
        return false;
    }

    if (startsWith(cookie.name, "__Host-") &&
        (!cookie.secure || !secureContext || cookie.domainSpecified ||
         !cookie.pathSpecified || cookie.path != "/")) {
        return false;
    }

    if (cookie.domainSpecified) {
        if (cookie.domain.empty()) {
            return false;
        }

        if (isIpAddress(normalizedOrigin.host)) {
            return false;
        }

        if (cookie_psl::isPublicSuffix(cookie.domain)) {
            return false;
        }

        if (!domainMatches(normalizedOrigin.host, cookie.domain)) {
            return false;
        }
    }

    return true;
}

int CookiePolicy::filterResponseCookies(HeaderMap& responseHeaders,
                                        const Origin& requestOrigin) {
    int removed = 0;
    const std::vector<std::string> cookieKeys = {"Set-Cookie", "set-cookie"};

    for (const std::string& key : cookieKeys) {
        auto it = responseHeaders.find(key);
        if (it == responseHeaders.end()) {
            continue;
        }

        auto parsed = parseSetCookie(it->second);
        if (!parsed.has_value() || !shouldAcceptCookie(parsed.value(), requestOrigin)) {
            responseHeaders.erase(it);
            ++removed;
        }
    }

    return removed;
}

int CookiePolicy::filterResponseCookies(std::vector<std::string>& setCookieHeaders,
                                        const Origin& requestOrigin) {
    const size_t before = setCookieHeaders.size();
    setCookieHeaders.erase(
        std::remove_if(setCookieHeaders.begin(),
                       setCookieHeaders.end(),
                       [&requestOrigin](const std::string& header) {
                           auto parsed = parseSetCookie(header);
                           return !parsed.has_value() ||
                                  !shouldAcceptCookie(parsed.value(), requestOrigin);
                       }),
        setCookieHeaders.end());

    return static_cast<int>(before - setCookieHeaders.size());
}

CookieJar::CookieJar(std::shared_ptr<ICookiePersistence> persistence)
    : persistence_(std::move(persistence)) {
    if (!persistence_) {
        return;
    }

    cookies_ = persistence_->load();
    const CookieTime now = std::chrono::system_clock::now();
    cookies_.erase(
        std::remove_if(cookies_.begin(),
                       cookies_.end(),
                       [now](const StoredCookie& cookie) { return cookieExpired(cookie, now); }),
        cookies_.end());
}

bool CookieJar::setCookieFromHeader(const std::string& requestUrl,
                                    const std::string& setCookieHeader,
                                    CookieTime now) {
    const ParsedUrl parsedUrl = parseUrl(requestUrl);
    if (!parsedUrl.valid) {
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    const ApplyCookieResult result = applySetCookie(cookies_, parsedUrl, setCookieHeader, now);
    if (result.changed) {
        persistLocked(now);
    }

    return result.accepted;
}

CookieSetResult CookieJar::setCookiesFromHeaders(
    const std::string& requestUrl,
    const std::vector<std::string>& setCookieHeaders,
    CookieTime now) {
    CookieSetResult result;
    const ParsedUrl parsedUrl = parseUrl(requestUrl);
    if (!parsedUrl.valid) {
        result.rejected = setCookieHeaders.size();
        return result;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    bool changed = false;
    for (const std::string& header : setCookieHeaders) {
        const ApplyCookieResult applied = applySetCookie(cookies_, parsedUrl, header, now);
        if (applied.accepted) {
            ++result.accepted;
        } else {
            ++result.rejected;
        }
        changed = changed || applied.changed;
    }

    if (changed) {
        persistLocked(now);
    }

    return result;
}

void CookieJar::storeFromResponse(const std::string& responseUrl,
                                  const std::vector<std::string>& setCookieHeaders,
                                  CookieTime now) {
    const ParsedUrl parsedUrl = parseUrl(responseUrl);
    if (!parsedUrl.valid) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    bool changed = false;
    for (const std::string& header : setCookieHeaders) {
        const ApplyCookieResult applied = applySetCookie(cookies_, parsedUrl, header, now);
        changed = changed || applied.changed;
    }

    if (changed) {
        persistLocked(now);
    }
}

std::vector<StoredCookie> CookieJar::getMatchingCookies(
    const std::string& requestUrl,
    const CookieRequestOptions& options) const {
    const ParsedUrl parsedUrl = parseUrl(requestUrl);
    if (!parsedUrl.valid) {
        return {};
    }

    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<StoredCookie> matches;
    for (const StoredCookie& cookie : cookies_) {
        if (cookieExpired(cookie, options.now)) {
            continue;
        }

        const bool domainOk = cookie.hostOnly
            ? parsedUrl.host == cookie.domain
            : domainMatches(parsedUrl.host, cookie.domain);
        if (!domainOk) {
            continue;
        }

        if (!pathMatches(parsedUrl.path, cookie.path)) {
            continue;
        }

        if (cookie.secure && !isSecureRequest(parsedUrl)) {
            continue;
        }

        if (cookie.httpOnly && !options.includeHttpOnly) {
            continue;
        }

        if (!sameSiteAllowsSimpleRequest(cookie, options.sameSiteContext)) {
            continue;
        }

        matches.push_back(cookie);
    }

    sortCookies(matches);
    return matches;
}

std::string CookieJar::getCookieHeader(const std::string& requestUrl,
                                       const CookieRequestOptions& options) const {
    const std::vector<StoredCookie> matches = getMatchingCookies(requestUrl, options);
    std::string header;
    for (const StoredCookie& cookie : matches) {
        if (!header.empty()) {
            header += "; ";
        }
        header += cookie.name;
        header += "=";
        header += cookie.value;
    }

    return header;
}

std::string CookieJar::getCookieHeader(const CookieRequestContext& context,
                                       CookieTime now) const {
    const ParsedUrl parsedUrl = parseUrl(context.requestUrl);
    if (!parsedUrl.valid || context.credentialsMode == CredentialsMode::Omit) {
        return "";
    }

    if (context.credentialsMode == CredentialsMode::SameOrigin &&
        context.siteForCookies.has_value()) {
        auto requestOrigin = Origin::fromUrl(context.requestUrl);
        if (requestOrigin.has_value() &&
            requestOrigin.value() != context.siteForCookies.value()) {
            return "";
        }
    }

    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<StoredCookie> matches;
    for (const StoredCookie& cookie : cookies_) {
        if (cookieExpired(cookie, now)) {
            continue;
        }

        const bool domainOk = cookie.hostOnly
            ? parsedUrl.host == cookie.domain
            : domainMatches(parsedUrl.host, cookie.domain);
        if (!domainOk) {
            continue;
        }

        if (!pathMatches(parsedUrl.path, cookie.path)) {
            continue;
        }

        if (cookie.secure && !isSecureRequest(parsedUrl)) {
            continue;
        }

        if (!sameSiteAllowsRequest(cookie, context, parsedUrl)) {
            continue;
        }

        matches.push_back(cookie);
    }

    sortCookies(matches);

    std::string header;
    for (const StoredCookie& cookie : matches) {
        if (!header.empty()) {
            header += "; ";
        }
        header += cookie.name;
        header += "=";
        header += cookie.value;
    }

    return header;
}

std::vector<StoredCookie> CookieJar::getAllCookies(CookieTime now) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<StoredCookie> active;
    for (const StoredCookie& cookie : cookies_) {
        if (!cookieExpired(cookie, now)) {
            active.push_back(cookie);
        }
    }
    return active;
}

std::vector<StoredCookie> CookieJar::snapshot(CookieTime now) const {
    return getAllCookies(now);
}

size_t CookieJar::purgeExpired(CookieTime now) {
    std::lock_guard<std::mutex> lock(mutex_);
    const size_t before = cookies_.size();
    cookies_.erase(
        std::remove_if(cookies_.begin(),
                       cookies_.end(),
                       [now](const StoredCookie& cookie) { return cookieExpired(cookie, now); }),
        cookies_.end());
    const size_t removed = before - cookies_.size();
    if (removed > 0) {
        persistLocked(now);
    }
    return removed;
}

size_t CookieJar::size(CookieTime now) const {
    return getAllCookies(now).size();
}

void CookieJar::clearSessionCookies() {
    const CookieTime now = std::chrono::system_clock::now();
    std::lock_guard<std::mutex> lock(mutex_);
    cookies_.erase(
        std::remove_if(cookies_.begin(),
                       cookies_.end(),
                       [](const StoredCookie& cookie) { return !cookie.persistent; }),
        cookies_.end());
    persistLocked(now);
}

void CookieJar::clearExpired(CookieTime now) {
    (void)purgeExpired(now);
}

void CookieJar::persistLocked(CookieTime now) {
    if (!persistence_) {
        return;
    }

    std::vector<StoredCookie> persistentCookies;
    persistentCookies.reserve(cookies_.size());
    for (const StoredCookie& cookie : cookies_) {
        if (cookie.persistent && !cookieExpired(cookie, now)) {
            persistentCookies.push_back(cookie);
        }
    }

    persistence_->save(persistentCookies);
}

} // namespace browser_network
