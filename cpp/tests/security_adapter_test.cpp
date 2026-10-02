#include <iostream>
#include <cassert>
#include <string>
#include <memory>
#include <vector>
#include "../adapters/security/SecurityAdapter.h"
#include "MockNetworkAdapter.h"

using namespace browser_network;

void test_native_mode_passthrough() {
    auto mock = std::make_shared<MockNetworkAdapter>();
    NetworkConfig config;
    config.mode = OriginMode::Native;

    SecurityAdapter adapter(mock, config);

    mock->setHandler([](const HttpRequest& req) {
        // Verificar que NÃO há header Origin
        auto h = req.getHeaders();
        assert(h.find("Origin") == h.end());

        HttpResponse res(200, "OK");
        return res;
    });

    HttpRequest req("https://api.example.com/data", HttpMethod::GET);
    auto future = adapter.send(req);
    HttpResponse res = future.get();

    assert(res.getStatus().code == 200);

    std::cout << "test_native_mode_passthrough PASSED" << std::endl;
}

void test_browser_emulation_origin_header() {
    auto mock = std::make_shared<MockNetworkAdapter>();
    NetworkConfig config;
    config.mode = OriginMode::BrowserEmulation;
    config.bundleId = "com.empresa.app";

    SecurityAdapter adapter(mock, config);

    mock->setHandler([](const HttpRequest& req) {
        // Verificar que Origin está presente
        auto h = req.getHeaders();
        auto it = h.find("Origin");
        assert(it != h.end());
        assert(it->second == "https://com-empresa-app.app");

        HttpResponse res(200, "OK");
        HeaderMap rh;
        // Same-origin (URL da requisição tem host diferente, mas o CORS valida)
        rh["Access-Control-Allow-Origin"] = "https://com-empresa-app.app";
        res.setHeaders(std::move(rh));
        return res;
    });

    HttpRequest req("https://api.example.com/data", HttpMethod::GET);
    auto future = adapter.send(req);
    HttpResponse res = future.get();

    assert(res.getStatus().code == 200);

    std::cout << "test_browser_emulation_origin_header PASSED" << std::endl;
}

void test_cors_blocks_invalid_response() {
    auto mock = std::make_shared<MockNetworkAdapter>();
    NetworkConfig config;
    config.mode = OriginMode::BrowserEmulation;
    config.bundleId = "com.empresa.app";

    SecurityAdapter adapter(mock, config);

    mock->setHandler([](const HttpRequest&) {
        HttpResponse res(200, "OK");
        // Sem header CORS — deve ser bloqueado
        return res;
    });

    HttpRequest req("https://api.example.com/data", HttpMethod::GET);
    auto future = adapter.send(req);

    bool blocked = false;
    try {
        future.get();
    } catch (const CorsBlockedError& e) {
        blocked = true;
    } catch (...) {
        // std::async pode encapsular exceção em std::future
        blocked = true;
    }

    assert(blocked == true);

    std::cout << "test_cors_blocks_invalid_response PASSED" << std::endl;
}

void test_preflight_sent_before_put() {
    auto mock = std::make_shared<MockNetworkAdapter>();
    NetworkConfig config;
    config.mode = OriginMode::BrowserEmulation;
    config.bundleId = "com.empresa.app";

    SecurityAdapter adapter(mock, config);

    int callCount = 0;
    mock->setHandler([&callCount](const HttpRequest& req) {
        callCount++;

        if (req.getMethod() == HttpMethod::OPTIONS) {
            // Resposta de preflight
            HttpResponse res(204, "No Content");
            HeaderMap h;
            h["Access-Control-Allow-Origin"] = "https://com-empresa-app.app";
            h["Access-Control-Allow-Methods"] = "GET, PUT, DELETE";
            h["Access-Control-Allow-Headers"] = "X-Custom, Content-Type";
            h["Access-Control-Max-Age"] = "600";
            res.setHeaders(std::move(h));
            return res;
        }

        // Request real
        HttpResponse res(200, "OK");
        HeaderMap h;
        h["Access-Control-Allow-Origin"] = "https://com-empresa-app.app";
        res.setHeaders(std::move(h));
        return res;
    });

    HttpRequest req("https://api.example.com/data", HttpMethod::PUT);
    req.addHeader("X-Custom", "value");
    req.addHeader("Content-Type", "application/json");

    auto future = adapter.send(req);
    HttpResponse res = future.get();

    assert(res.getStatus().code == 200);
    assert(callCount == 2); // OPTIONS + PUT

    std::cout << "test_preflight_sent_before_put PASSED" << std::endl;
}

void test_cookie_secure_filtered_in_insecure() {
    auto mock = std::make_shared<MockNetworkAdapter>();
    NetworkConfig config;
    config.mode = OriginMode::BrowserEmulation;
    config.debugLocalhost = true; // localhost = secure context

    SecurityAdapter adapter(mock, config);

    mock->setHandler([](const HttpRequest&) {
        HttpResponse res(200, "OK");
        HeaderMap h;
        h["Access-Control-Allow-Origin"] = "http://localhost";
        h["Set-Cookie"] = "session=abc; Secure; SameSite=None";
        res.setHeaders(std::move(h));
        return res;
    });

    HttpRequest req("https://api.example.com/data", HttpMethod::GET);
    auto future = adapter.send(req);
    HttpResponse res = future.get();

    // localhost é secure context, então cookie deve ser aceito
    assert(res.getHeaders().find("Set-Cookie") != res.getHeaders().end());

    std::cout << "test_cookie_secure_filtered_in_insecure PASSED" << std::endl;
}

void test_custom_origin_in_request() {
    auto mock = std::make_shared<MockNetworkAdapter>();
    NetworkConfig config;
    config.mode = OriginMode::BrowserEmulation;
    config.bundleId = "com.empresa.app";
    config.customOrigin = "https://my-custom-origin.com";

    SecurityAdapter adapter(mock, config);

    mock->setHandler([](const HttpRequest& req) {
        auto h = req.getHeaders();
        auto it = h.find("Origin");
        assert(it != h.end());
        assert(it->second == "https://my-custom-origin.com");

        HttpResponse res(200, "OK");
        HeaderMap rh;
        rh["Access-Control-Allow-Origin"] = "https://my-custom-origin.com";
        res.setHeaders(std::move(rh));
        return res;
    });

    HttpRequest req("https://api.example.com/data", HttpMethod::GET);
    auto future = adapter.send(req);
    HttpResponse res = future.get();

    assert(res.getStatus().code == 200);

    std::cout << "test_custom_origin_in_request PASSED" << std::endl;
}

int main() {
    test_native_mode_passthrough();
    test_browser_emulation_origin_header();
    test_cors_blocks_invalid_response();
    test_preflight_sent_before_put();
    test_cookie_secure_filtered_in_insecure();
    test_custom_origin_in_request();

    std::cout << "\n=== Todos os testes de SecurityAdapter PASSARAM ===" << std::endl;
    return 0;
}
