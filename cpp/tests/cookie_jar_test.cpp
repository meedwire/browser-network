#include <cassert>
#include <chrono>
#include <iostream>
#include <string>
#include <vector>

#include "../core/security/CookieJar.h"
#include "../core/security/CookiePolicy.h"

using namespace browser_network;

namespace {

CookieTime atSeconds(int64_t seconds) {
    return CookieTime(std::chrono::seconds(seconds));
}

CookieRequestOptions makeOptions(CookieTime now, CookieSameSiteContext context) {
    CookieRequestOptions options;
    options.now = now;
    options.sameSiteContext = context;
    return options;
}

void test_multi_set_cookie_storage() {
    CookieJar jar;
    CookieTime now = atSeconds(1'770'000'000);

    CookieSetResult result = jar.setCookiesFromHeaders(
        "https://app.example.com/login",
        {
            "session=abc; Path=/; HttpOnly",
            "theme=dark; Path=/; Secure",
            "broken; Path=/",
        },
        now);

    assert(result.accepted == 2);
    assert(result.rejected == 1);
    assert(jar.size(now) == 2);
    assert(jar.getCookieHeader("https://app.example.com/home",
                               makeOptions(now, CookieSameSiteContext::Strict)) ==
           "session=abc; theme=dark");

    std::cout << "test_multi_set_cookie_storage PASSED" << std::endl;
}

void test_domain_and_host_only_matching() {
    CookieJar jar;
    CookieTime now = atSeconds(1'770'000'100);

    assert(jar.setCookieFromHeader(
        "https://app.example.com/login",
        "hostOnly=1; Path=/",
        now));
    assert(jar.setCookieFromHeader(
        "https://app.example.com/login",
        "shared=1; Domain=example.com; Path=/",
        now));

    assert(jar.getCookieHeader("https://app.example.com/dashboard",
                               makeOptions(now, CookieSameSiteContext::Strict)) ==
           "hostOnly=1; shared=1");
    assert(jar.getCookieHeader("https://api.example.com/dashboard",
                               makeOptions(now, CookieSameSiteContext::Strict)) ==
           "shared=1");

    std::cout << "test_domain_and_host_only_matching PASSED" << std::endl;
}

void test_path_matching_and_ordering() {
    CookieJar jar;
    CookieTime now = atSeconds(1'770'000'200);

    assert(jar.setCookieFromHeader(
        "https://example.com/account/login",
        "root=r; Path=/",
        now));
    assert(jar.setCookieFromHeader(
        "https://example.com/account/login",
        "account=a; Path=/account",
        now + std::chrono::seconds(1)));

    assert(jar.getCookieHeader("https://example.com/account/settings",
                               makeOptions(now + std::chrono::seconds(1),
                                           CookieSameSiteContext::Strict)) ==
           "account=a; root=r");
    assert(jar.getCookieHeader("https://example.com/accounting",
                               makeOptions(now + std::chrono::seconds(1),
                                           CookieSameSiteContext::Strict)) ==
           "root=r");

    std::cout << "test_path_matching_and_ordering PASSED" << std::endl;
}

void test_max_age_and_expires() {
    CookieJar jar;
    CookieTime now = atSeconds(1'770'000'300);

    assert(jar.setCookieFromHeader(
        "https://example.com/login",
        "temp=1; Max-Age=10; Expires=Wed, 21 Oct 2030 07:28:00 GMT; Path=/",
        now));
    assert(jar.setCookieFromHeader(
        "https://example.com/login",
        "future=1; Expires=Wed, 21 Oct 2030 07:28:00 GMT; Path=/",
        now));

    assert(jar.getCookieHeader("https://example.com/app",
                               makeOptions(now + std::chrono::seconds(9),
                                           CookieSameSiteContext::Strict)) ==
           "temp=1; future=1");
    assert(jar.getCookieHeader("https://example.com/app",
                               makeOptions(now + std::chrono::seconds(11),
                                           CookieSameSiteContext::Strict)) ==
           "future=1");

    assert(jar.setCookieFromHeader(
        "https://example.com/login",
        "future=1; Max-Age=0; Path=/",
        now + std::chrono::seconds(12)));
    assert(jar.size(now + std::chrono::seconds(12)) == 0);

    std::cout << "test_max_age_and_expires PASSED" << std::endl;
}

void test_samesite_matching() {
    CookieJar jar;
    CookieTime now = atSeconds(1'770'000'400);

    assert(jar.setCookieFromHeader(
        "https://example.com/",
        "strict=1; Path=/; SameSite=Strict",
        now));
    assert(jar.setCookieFromHeader(
        "https://example.com/",
        "lax=1; Path=/; SameSite=Lax",
        now));
    assert(jar.setCookieFromHeader(
        "https://example.com/",
        "defaulted=1; Path=/",
        now));
    assert(jar.setCookieFromHeader(
        "https://example.com/",
        "none=1; Path=/; SameSite=None; Secure",
        now));

    assert(jar.getCookieHeader("https://example.com/",
                               makeOptions(now, CookieSameSiteContext::Strict)) ==
           "strict=1; lax=1; defaulted=1; none=1");
    assert(jar.getCookieHeader("https://example.com/",
                               makeOptions(now, CookieSameSiteContext::Lax)) ==
           "lax=1; defaulted=1; none=1");
    assert(jar.getCookieHeader("https://example.com/",
                               makeOptions(now, CookieSameSiteContext::CrossSite)) ==
           "none=1");

    std::cout << "test_samesite_matching PASSED" << std::endl;
}

void test_prefix_rules() {
    CookieJar jar;
    CookieTime now = atSeconds(1'770'000'500);

    assert(!jar.setCookieFromHeader(
        "https://example.com/",
        "__Secure-id=1; Path=/",
        now));
    assert(!jar.setCookieFromHeader(
        "http://example.com/",
        "__Secure-id=1; Path=/; Secure",
        now));
    assert(!jar.setCookieFromHeader(
        "https://example.com/",
        "__Host-id=1; Secure",
        now));
    assert(!jar.setCookieFromHeader(
        "https://example.com/",
        "__Host-id=1; Secure; Path=/; Domain=example.com",
        now));
    assert(jar.setCookieFromHeader(
        "https://example.com/",
        "__Host-id=1; Secure; Path=/",
        now));

    assert(jar.getCookieHeader("https://example.com/",
                               makeOptions(now, CookieSameSiteContext::Strict)) ==
           "__Host-id=1");
    assert(jar.getCookieHeader("https://sub.example.com/",
                               makeOptions(now, CookieSameSiteContext::Strict)).empty());

    std::cout << "test_prefix_rules PASSED" << std::endl;
}

void test_public_suffix_rejection() {
    CookieJar jar;
    CookieTime now = atSeconds(1'770'000'600);

    assert(!jar.setCookieFromHeader(
        "https://shop.example.co.uk/login",
        "bad=1; Domain=co.uk; Path=/",
        now));
    assert(jar.setCookieFromHeader(
        "https://shop.example.co.uk/login",
        "good=1; Domain=example.co.uk; Path=/",
        now));
    assert(jar.getCookieHeader("https://cdn.example.co.uk/",
                               makeOptions(now, CookieSameSiteContext::Strict)) ==
           "good=1");

    std::cout << "test_public_suffix_rejection PASSED" << std::endl;
}

void test_cookie_policy_multi_filter() {
    std::vector<std::string> headers = {
        "ok=1; Secure; SameSite=None",
        "drop=1; SameSite=None",
        "__Host-token=1; Secure; Path=/",
    };

    Origin origin;
    origin.scheme = "https";
    origin.host = "example.com";
    origin.port = 443;

    int removed = CookiePolicy::filterResponseCookies(headers, origin);
    assert(removed == 1);
    assert(headers.size() == 2);
    assert(headers[0] == "ok=1; Secure; SameSite=None");
    assert(headers[1] == "__Host-token=1; Secure; Path=/");

    std::cout << "test_cookie_policy_multi_filter PASSED" << std::endl;
}

} // namespace

int main() {
    test_multi_set_cookie_storage();
    test_domain_and_host_only_matching();
    test_path_matching_and_ordering();
    test_max_age_and_expires();
    test_samesite_matching();
    test_prefix_rules();
    test_public_suffix_rejection();
    test_cookie_policy_multi_filter();

    std::cout << "\n=== Todos os testes de CookieJar PASSARAM ===" << std::endl;
    return 0;
}
