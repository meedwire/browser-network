#include <iostream>
#include <cassert>
#include <string>
#include "../core/security/CorsPolicy.h"
#include "../core/security/CorsPreflightCache.h"
#include "../core/security/Origin.h"
#include "../core/HttpRequest.h"

using namespace browser_network;

void test_simple_request_get() {
    HeaderMap headers;
    assert(CorsPolicy::isSimpleRequest(HttpMethod::GET, headers) == true);

    std::cout << "test_simple_request_get PASSED" << std::endl;
}

void test_simple_request_post_form() {
    HeaderMap headers;
    headers["Content-Type"] = "application/x-www-form-urlencoded";
    assert(CorsPolicy::isSimpleRequest(HttpMethod::POST, headers) == true);

    std::cout << "test_simple_request_post_form PASSED" << std::endl;
}

void test_preflight_custom_header() {
    HeaderMap headers;
    headers["X-Custom-Header"] = "value";
    assert(CorsPolicy::isSimpleRequest(HttpMethod::GET, headers) == false);

    std::cout << "test_preflight_custom_header PASSED" << std::endl;
}

void test_preflight_put_method() {
    HeaderMap headers;
    assert(CorsPolicy::isSimpleRequest(HttpMethod::PUT, headers) == false);

    std::cout << "test_preflight_put_method PASSED" << std::endl;
}

void test_preflight_json_content_type() {
    HeaderMap headers;
    headers["Content-Type"] = "application/json";
    assert(CorsPolicy::isSimpleRequest(HttpMethod::POST, headers) == false);

    std::cout << "test_preflight_json_content_type PASSED" << std::endl;
}

void test_preflight_required_via_request() {
    HttpRequest req("https://api.example.com/data", HttpMethod::PUT);
    req.addHeader("X-API-Key", "abc");
    assert(CorsPolicy::isPreflightRequired(req) == true);

    HttpRequest simple("https://api.example.com/data", HttpMethod::GET);
    assert(CorsPolicy::isPreflightRequired(simple) == false);

    std::cout << "test_preflight_required_via_request PASSED" << std::endl;
}

void test_cors_response_valid() {
    Origin origin;
    origin.scheme = "https";
    origin.host = "com-empresa-app.app";

    HeaderMap responseHeaders;
    responseHeaders["Access-Control-Allow-Origin"] = "https://com-empresa-app.app";

    auto result = CorsPolicy::validateResponse(origin, responseHeaders);
    assert(result.allowed == true);

    std::cout << "test_cors_response_valid PASSED" << std::endl;
}

void test_cors_response_invalid() {
    Origin origin;
    origin.scheme = "https";
    origin.host = "com-empresa-app.app";

    HeaderMap responseHeaders;
    responseHeaders["Access-Control-Allow-Origin"] = "https://other.com";

    auto result = CorsPolicy::validateResponse(origin, responseHeaders);
    assert(result.allowed == false);
    assert(!result.blockReason.empty());

    std::cout << "test_cors_response_invalid PASSED" << std::endl;
}

void test_cors_response_missing() {
    Origin origin;
    origin.scheme = "https";
    origin.host = "example.com";

    HeaderMap responseHeaders; // sem CORS headers

    auto result = CorsPolicy::validateResponse(origin, responseHeaders);
    assert(result.allowed == false);

    std::cout << "test_cors_response_missing PASSED" << std::endl;
}

void test_cors_wildcard() {
    Origin origin;
    origin.scheme = "https";
    origin.host = "example.com";

    HeaderMap responseHeaders;
    responseHeaders["Access-Control-Allow-Origin"] = "*";

    auto result = CorsPolicy::validateResponse(origin, responseHeaders, false);
    assert(result.allowed == true);

    std::cout << "test_cors_wildcard PASSED" << std::endl;
}

void test_cors_wildcard_with_credentials() {
    Origin origin;
    origin.scheme = "https";
    origin.host = "example.com";

    HeaderMap responseHeaders;
    responseHeaders["Access-Control-Allow-Origin"] = "*";

    auto result = CorsPolicy::validateResponse(origin, responseHeaders, true);
    assert(result.allowed == false); // Wildcard + credentials = bloqueado

    std::cout << "test_cors_wildcard_with_credentials PASSED" << std::endl;
}

void test_preflight_validation() {
    Origin origin;
    origin.scheme = "https";
    origin.host = "com-empresa-app.app";

    HeaderMap requestHeaders;
    requestHeaders["X-Custom"] = "value";

    HeaderMap preflightResponse;
    preflightResponse["Access-Control-Allow-Origin"] = "https://com-empresa-app.app";
    preflightResponse["Access-Control-Allow-Methods"] = "GET, PUT, DELETE";
    preflightResponse["Access-Control-Allow-Headers"] = "X-Custom, Authorization";
    preflightResponse["Access-Control-Max-Age"] = "3600";

    auto result = CorsPolicy::validatePreflight(
        origin, HttpMethod::PUT, requestHeaders, preflightResponse);

    assert(result.allowed == true);
    assert(result.maxAge == 3600);

    std::cout << "test_preflight_validation PASSED" << std::endl;
}

void test_preflight_method_rejected() {
    Origin origin;
    origin.scheme = "https";
    origin.host = "com-empresa-app.app";

    HeaderMap preflightResponse;
    preflightResponse["Access-Control-Allow-Origin"] = "https://com-empresa-app.app";
    preflightResponse["Access-Control-Allow-Methods"] = "GET, POST";

    auto result = CorsPolicy::validatePreflight(
        origin, HttpMethod::DELETE, {}, preflightResponse);

    assert(result.allowed == false);

    std::cout << "test_preflight_method_rejected PASSED" << std::endl;
}

void test_preflight_header_rejected() {
    Origin origin;
    origin.scheme = "https";
    origin.host = "com-empresa-app.app";

    HeaderMap requestHeaders;
    requestHeaders["X-Secret"] = "value";

    HeaderMap preflightResponse;
    preflightResponse["Access-Control-Allow-Origin"] = "https://com-empresa-app.app";
    preflightResponse["Access-Control-Allow-Methods"] = "PUT";
    preflightResponse["Access-Control-Allow-Headers"] = "X-Other";

    auto result = CorsPolicy::validatePreflight(
        origin, HttpMethod::PUT, requestHeaders, preflightResponse);

    assert(result.allowed == false);

    std::cout << "test_preflight_header_rejected PASSED" << std::endl;
}

void test_preflight_cache() {
    CorsPreflightCache cache;
    Origin origin;
    origin.scheme = "https";
    origin.host = "example.com";

    assert(cache.hasValidEntry(origin, "https://api.com/data", "PUT") == false);

    cache.store(origin, "https://api.com/data", "PUT", "PUT", "X-Custom", 60);

    assert(cache.hasValidEntry(origin, "https://api.com/data", "PUT") == true);
    assert(cache.hasValidEntry(origin, "https://api.com/other", "PUT") == false);

    cache.clear();
    assert(cache.hasValidEntry(origin, "https://api.com/data", "PUT") == false);

    std::cout << "test_preflight_cache PASSED" << std::endl;
}

int main() {
    // Simple request detection
    test_simple_request_get();
    test_simple_request_post_form();
    test_preflight_custom_header();
    test_preflight_put_method();
    test_preflight_json_content_type();
    test_preflight_required_via_request();

    // CORS response validation
    test_cors_response_valid();
    test_cors_response_invalid();
    test_cors_response_missing();
    test_cors_wildcard();
    test_cors_wildcard_with_credentials();

    // Preflight validation
    test_preflight_validation();
    test_preflight_method_rejected();
    test_preflight_header_rejected();
    test_preflight_cache();

    std::cout << "\n=== Todos os testes de CorsPolicy PASSARAM ===" << std::endl;
    return 0;
}
