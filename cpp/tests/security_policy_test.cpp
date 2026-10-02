#include <iostream>
#include <cassert>
#include <string>
#include "../core/security/Origin.h"
#include "../core/security/NetworkConfig.h"
#include "../core/security/SecurityPolicy.h"
#include "../core/security/CookiePolicy.h"

using namespace browser_network;

void test_synthetic_origin_from_bundle_id() {
    Origin o = SecurityPolicy::deriveSyntheticOrigin("com.empresa.app");
    assert(o.scheme == "https");
    assert(o.host == "com-empresa-app.app");
    assert(o.port == 443);
    assert(o.serialize() == "https://com-empresa-app.app");

    std::cout << "test_synthetic_origin_from_bundle_id PASSED" << std::endl;
}

void test_synthetic_origin_uppercase() {
    Origin o = SecurityPolicy::deriveSyntheticOrigin("com.Empresa.MeuApp");
    assert(o.host == "com-empresa-meuapp.app");
    assert(o.serialize() == "https://com-empresa-meuapp.app");

    std::cout << "test_synthetic_origin_uppercase PASSED" << std::endl;
}

void test_synthetic_origin_special_chars() {
    Origin o = SecurityPolicy::deriveSyntheticOrigin("com.empresa@#$.app");
    // Caracteres inválidos removidos
    assert(o.host == "com-empresa-app.app");

    std::cout << "test_synthetic_origin_special_chars PASSED" << std::endl;
}

void test_debug_localhost_origin() {
    NetworkConfig config;
    config.mode = OriginMode::BrowserEmulation;
    config.debugLocalhost = true;

    Origin o = SecurityPolicy::resolveOrigin(config);
    assert(o.scheme == "http");
    assert(o.host == "localhost");
    assert(o.serialize() == "http://localhost");

    std::cout << "test_debug_localhost_origin PASSED" << std::endl;
}

void test_custom_origin_override() {
    NetworkConfig config;
    config.mode = OriginMode::BrowserEmulation;
    config.bundleId = "com.empresa.app";
    config.debugLocalhost = true;
    config.customOrigin = "https://custom.example.com:9090";

    Origin o = SecurityPolicy::resolveOrigin(config);
    // Custom tem maior prioridade
    assert(o.scheme == "https");
    assert(o.host == "custom.example.com");
    assert(o.port == 9090);
    assert(o.serialize() == "https://custom.example.com:9090");

    std::cout << "test_custom_origin_override PASSED" << std::endl;
}

void test_secure_context_https() {
    Origin o;
    o.scheme = "https";
    o.host = "example.com";
    assert(SecurityPolicy::isSecureContext(o) == true);

    std::cout << "test_secure_context_https PASSED" << std::endl;
}

void test_secure_context_localhost() {
    Origin o;
    o.scheme = "http";
    o.host = "localhost";
    assert(SecurityPolicy::isSecureContext(o) == true);

    // 127.0.0.1 também é secure
    o.host = "127.0.0.1";
    assert(SecurityPolicy::isSecureContext(o) == true);

    // ::1 IPv6 loopback
    o.host = "::1";
    assert(SecurityPolicy::isSecureContext(o) == true);

    std::cout << "test_secure_context_localhost PASSED" << std::endl;
}

void test_insecure_context_http() {
    Origin o;
    o.scheme = "http";
    o.host = "example.com";
    assert(SecurityPolicy::isSecureContext(o) == false);

    std::cout << "test_insecure_context_http PASSED" << std::endl;
}

void test_origin_from_url() {
    auto o = Origin::fromUrl("https://api.example.com:8080/path/to/resource");
    assert(o.has_value());
    assert(o->scheme == "https");
    assert(o->host == "api.example.com");
    assert(o->port == 8080);

    auto o2 = Origin::fromUrl("http://localhost/api");
    assert(o2.has_value());
    assert(o2->host == "localhost");
    assert(o2->port == 0);
    assert(o2->effectivePort() == 80);

    std::cout << "test_origin_from_url PASSED" << std::endl;
}

void test_same_origin() {
    Origin a;
    a.scheme = "https"; a.host = "example.com"; a.port = 443;
    Origin b;
    b.scheme = "https"; b.host = "example.com"; b.port = 0; // 0 → 443

    assert(SecurityPolicy::isSameOrigin(a, b) == true);

    Origin c;
    c.scheme = "http"; c.host = "example.com"; c.port = 80;
    assert(SecurityPolicy::isSameOrigin(a, c) == false);

    std::cout << "test_same_origin PASSED" << std::endl;
}

// --- Cookie Security Tests ---

void test_secure_cookie_rejected_in_insecure() {
    CookieAttributes cookie;
    cookie.name = "session";
    cookie.value = "abc123";
    cookie.secure = true;

    Origin insecure;
    insecure.scheme = "http";
    insecure.host = "example.com";

    assert(CookiePolicy::shouldAcceptCookie(cookie, insecure) == false);

    std::cout << "test_secure_cookie_rejected_in_insecure PASSED" << std::endl;
}

void test_secure_cookie_accepted_in_https() {
    CookieAttributes cookie;
    cookie.name = "session";
    cookie.value = "abc123";
    cookie.secure = true;

    Origin secure;
    secure.scheme = "https";
    secure.host = "example.com";

    assert(CookiePolicy::shouldAcceptCookie(cookie, secure) == true);

    std::cout << "test_secure_cookie_accepted_in_https PASSED" << std::endl;
}

void test_secure_cookie_accepted_in_localhost() {
    CookieAttributes cookie;
    cookie.name = "session";
    cookie.value = "abc123";
    cookie.secure = true;

    Origin localhost;
    localhost.scheme = "http";
    localhost.host = "localhost";

    assert(CookiePolicy::shouldAcceptCookie(cookie, localhost) == true);

    std::cout << "test_secure_cookie_accepted_in_localhost PASSED" << std::endl;
}

void test_samesite_none_requires_secure() {
    CookieAttributes cookie;
    cookie.name = "tracking";
    cookie.value = "xyz";
    cookie.sameSite = "None";
    cookie.secure = false; // SameSite=None sem Secure

    Origin secure;
    secure.scheme = "https";
    secure.host = "example.com";

    assert(CookiePolicy::shouldAcceptCookie(cookie, secure) == false);

    // Com Secure = true, deve aceitar
    cookie.secure = true;
    assert(CookiePolicy::shouldAcceptCookie(cookie, secure) == true);

    std::cout << "test_samesite_none_requires_secure PASSED" << std::endl;
}

void test_parse_set_cookie() {
    auto cookie = CookiePolicy::parseSetCookie(
        "session=abc123; Secure; HttpOnly; SameSite=None; Domain=example.com; Path=/");

    assert(cookie.has_value());
    assert(cookie->name == "session");
    assert(cookie->value == "abc123");
    assert(cookie->secure == true);
    assert(cookie->httpOnly == true);
    assert(cookie->sameSite == "None");
    assert(cookie->domain == "example.com");
    assert(cookie->path == "/");

    std::cout << "test_parse_set_cookie PASSED" << std::endl;
}

void test_filter_insecure_cookies() {
    HeaderMap headers;
    headers["Set-Cookie"] = "session=abc123; Secure; SameSite=None";

    Origin insecure;
    insecure.scheme = "http";
    insecure.host = "example.com";

    int removed = CookiePolicy::filterResponseCookies(headers, insecure);
    assert(removed == 1);
    assert(headers.find("Set-Cookie") == headers.end());

    std::cout << "test_filter_insecure_cookies PASSED" << std::endl;
}

int main() {
    // Origin tests
    test_synthetic_origin_from_bundle_id();
    test_synthetic_origin_uppercase();
    test_synthetic_origin_special_chars();
    test_debug_localhost_origin();
    test_custom_origin_override();
    test_secure_context_https();
    test_secure_context_localhost();
    test_insecure_context_http();
    test_origin_from_url();
    test_same_origin();

    // Cookie tests
    test_secure_cookie_rejected_in_insecure();
    test_secure_cookie_accepted_in_https();
    test_secure_cookie_accepted_in_localhost();
    test_samesite_none_requires_secure();
    test_parse_set_cookie();
    test_filter_insecure_cookies();

    std::cout << "\n=== Todos os testes de SecurityPolicy PASSARAM ===" << std::endl;
    return 0;
}
