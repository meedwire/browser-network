#include <iostream>
#include <cassert>
#include <memory>
#include <thread>
#include <string>
#include <vector>
#include "../adapters/cache/CachedNetworkAdapter.h"
#include "../adapters/cache/InMemoryCacheStorage.h"
#include "../core/utils/HttpDate.h"
#include "MockNetworkAdapter.h"

using namespace browser_network;

// Helper to convert string to ByteBuffer
ByteBuffer toByteBuffer(const std::string& s) {
    return ByteBuffer(s.begin(), s.end());
}

// Helper to convert ByteBuffer to string
std::string toString(const ByteBuffer& b) {
    return std::string(b.begin(), b.end());
}

void test_cache_miss_stores_response() {
    auto mock = std::make_shared<MockNetworkAdapter>();
    auto storage = std::make_shared<InMemoryCacheStorage>();
    auto cache = std::make_shared<HttpCache>(storage);
    CachedNetworkAdapter adapter(mock, cache);

    // Configurar Mock para retornar 200 OK com max-age
    mock->setHandler([&](const HttpRequest& req) {
        HttpResponse res(200, "OK");
        HeaderMap h;
        h["Cache-Control"] = "max-age=60";
        h["Date"] = "Wed, 21 Oct 2025 07:28:00 GMT"; 
        res.setHeaders(std::move(h));
        res.setBody(toByteBuffer("Hello Cache"));
        return res;
    });

    HttpRequest req("http://example.com/1", HttpMethod::GET);
    
    // 1. Primeira chamada (Miss -> Network -> Store)
    auto future1 = adapter.send(req);
    HttpResponse res1 = future1.get();
    
    assert(res1.getStatus().code == 200);
    assert(toString(res1.getBody()) == "Hello Cache");
    
    // Verificar se armazenou
    auto entry = storage->get("http://example.com/1");
    assert(entry.has_value());
    
    std::cout << "test_cache_miss_stores_response PASSED" << std::endl;
}

void test_cache_hit_bypass_network() {
    auto mock = std::make_shared<MockNetworkAdapter>();
    auto storage = std::make_shared<InMemoryCacheStorage>();
    auto cache = std::make_shared<HttpCache>(storage);
    CachedNetworkAdapter adapter(mock, cache);

    // Pré-popular cache
    CacheEntry entry;
    entry.url = "http://example.com/hit";
    entry.method = HttpMethod::GET;
    entry.status = 200;
    entry.statusMessage = "OK Cached";
    entry.responseBody = toByteBuffer("Cached Body");
    entry.metadata.storedAt = std::chrono::system_clock::now();
    entry.metadata.responseTime = std::chrono::system_clock::now();
    entry.metadata.requestTime = std::chrono::system_clock::now();
    // Freshness infinita para garantir HIT
    entry.metadata.cacheControl = "max-age=3600"; 
    entry.metadata.dateValue = std::chrono::system_clock::now();
    
    storage->put(entry.url, entry);

    // Configurar Mock para falhar se chamado (ou retornar diferente)
    mock->setHandler([&](const HttpRequest&) {
        assert(false && "Network should not be called on HIT");
        return HttpResponse(500, "Error");
    });

    HttpRequest req("http://example.com/hit", HttpMethod::GET);
    auto future = adapter.send(req);
    HttpResponse res = future.get();
    
    assert(res.getStatus().code == 200);
    assert(toString(res.getBody()) == "Cached Body");
    
    HeaderMap h = res.getHeaders();
    assert(h["X-Cache"] == "HIT");

    std::cout << "test_cache_hit_bypass_network PASSED" << std::endl;
}

void test_revalidation_304() {
    auto mock = std::make_shared<MockNetworkAdapter>();
    auto storage = std::make_shared<InMemoryCacheStorage>();
    auto cache = std::make_shared<HttpCache>(storage);
    CachedNetworkAdapter adapter(mock, cache);

    // Pré-popular cache STALE
    CacheEntry entry;
    entry.url = "http://example.com/stale";
    entry.method = HttpMethod::GET;
    entry.status = 200;
    entry.responseBody = toByteBuffer("Old Body");
    entry.metadata.etag = "\"v1\"";
    // Forçar Stale: max-age=1, date=10s atrás
    entry.metadata.cacheControl = "max-age=1";
    auto now = std::chrono::system_clock::now();
    entry.metadata.dateValue = now - std::chrono::seconds(10);
    entry.metadata.responseTime = now - std::chrono::seconds(10);
    entry.metadata.requestTime = now - std::chrono::seconds(10);
    
    storage->put(entry.url, entry);

    // Mock retorna 304 Not Modified
    mock->setHandler([&](const HttpRequest& req) {
        // Verificar headers condicionais
        auto h = req.getHeaders();
        auto it = h.find("If-None-Match");
        assert(it != h.end());
        assert(it->second == "\"v1\"");
        
        HttpResponse res(304, "Not Modified");
        // Atualizar headers?
        HeaderMap hRes;
        hRes["Cache-Control"] = "max-age=60"; // Atualizar frescor
        hRes["Date"] = HttpDate::format(std::chrono::system_clock::now());
        res.setHeaders(std::move(hRes));
        return res;
    });

    HttpRequest req("http://example.com/stale", HttpMethod::GET);
    auto future = adapter.send(req);
    HttpResponse res = future.get();
    
    assert(res.getStatus().code == 200); // 304 transformado em 200 do cache
    assert(toString(res.getBody()) == "Old Body");
    
    // Verificar se cache foi atualizado (novo Date/max-age)
    auto newEntryOpt = storage->get(entry.url);
    assert(newEntryOpt->metadata.cacheControl == "max-age=60");

    std::cout << "test_revalidation_304 PASSED" << std::endl;
}

int main() {
    test_cache_miss_stores_response();
    test_cache_hit_bypass_network();
    test_revalidation_304();
    
    std::cout << "\n=== Todos os testes de CachedNetworkAdapter PASSARAM ===" << std::endl;
    return 0;
}
