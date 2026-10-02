#include <iostream>
#include <cassert>
#include <thread>
#include <chrono>
#include "../core/cache/CachePolicy.h"
#include "../core/cache/CacheEntry.h"
#include "../core/utils/HttpDate.h"

using namespace browser_network;

void test_is_cacheable() {
    HttpRequest req("http://example.com", HttpMethod::GET);
    HttpResponse res(200, "OK");
    
    // 1. Basic 200 OK -> Cacheable
    assert(CachePolicy::isCacheable(req, res) == true);

    // 2. POST method -> Not Cacheable (por enquanto)
    HttpRequest reqPost("http://example.com", HttpMethod::POST);
    assert(CachePolicy::isCacheable(reqPost, res) == false);

    // 3. Status 500 -> Not Cacheable
    HttpResponse res500(500, "Internal Server Error");
    assert(CachePolicy::isCacheable(req, res500) == false);

    // 4. Cache-Control: no-store -> Not Cacheable
    HttpResponse resNoStore(200, "OK");
    HeaderMap headersNoStore;
    headersNoStore["Cache-Control"] = "no-store";
    resNoStore.setHeaders(std::move(headersNoStore));
    assert(CachePolicy::isCacheable(req, resNoStore) == false);

    // 5. Authorization header -> Not Cacheable unless public/s-maxage
    HttpRequest reqAuth("http://example.com", HttpMethod::GET);
    reqAuth.addHeader("Authorization", "Bearer 123");
    assert(CachePolicy::isCacheable(reqAuth, res) == false); // Default private

    HttpResponse resPublic(200, "OK");
    HeaderMap headersPublic;
    headersPublic["Cache-Control"] = "public, max-age=60";
    resPublic.setHeaders(std::move(headersPublic));
    assert(CachePolicy::isCacheable(reqAuth, resPublic) == true); // Explicit public

    std::cout << "test_is_cacheable PASSED" << std::endl;
}

void test_compute_age() {
    using namespace std::chrono;
    
    CacheEntry entry;
    auto now = Clock::now();
    
    // Request sent at T-10s
    entry.metadata.requestTime = now - seconds(10);
    // Response received at T-9s (1s delay)
    entry.metadata.responseTime = now - seconds(9);
    // Date header says T-12s (server clock slow or pre-generated)
    entry.metadata.dateValue = now - seconds(12);
    // Age header says 5s
    entry.metadata.ageValue = 5;
    
    // apparent_age = max(0, responseTime - dateValue) = max(0, (-9) - (-12)) = 3s
    // response_delay = responseTime - requestTime = (-9) - (-10) = 1s
    // corrected_age_value = ageValue + responseDelay = 5 + 1 = 6s
    // corrected_initial_age = max(apparent, corrected_age) = max(3, 6) = 6s
    // resident_time = now - responseTime = 0 - (-9) = 9s
    // current_age = corrected_initial + resident_time = 6 + 9 = 15s
    
    auto age = CachePolicy::computeAge(entry, now);
    // Tolerância para execução
    assert(age.count() >= 15 && age.count() <= 16); 

    std::cout << "test_compute_age PASSED" << std::endl;
}

void test_freshness_check() {
    using namespace std::chrono;
    
    auto now = Clock::now();
    CacheEntry entry;
    entry.url = "http://example.com";
    entry.method = HttpMethod::GET;
    entry.metadata.requestTime = now - seconds(10);
    entry.metadata.responseTime = now - seconds(10);
    entry.metadata.dateValue = now - seconds(10);
    
    // max-age = 5s
    // current_age ~ 10s (resident time)
    // 10 < 5 -> FALSE (Stale)
    entry.metadata.cacheControl = "max-age=5";

    HttpRequest req("http://example.com", HttpMethod::GET);
    CacheResponseStatus status = CachePolicy::checkStatus(entry, req, now);
    assert(status == CacheResponseStatus::Stale);
    
    // max-age = 15s
    // current_age ~ 10s
    // 10 < 15 -> TRUE (Fresh)
    entry.metadata.cacheControl = "max-age=15";
    status = CachePolicy::checkStatus(entry, req, now);
    assert(status == CacheResponseStatus::Fresh);
    
    std::cout << "test_freshness_check PASSED" << std::endl;
}

void test_heuristic_freshness() {
    using namespace std::chrono;
    auto now = Clock::now();
    
    CacheEntry entry;
    entry.metadata.cacheControl = ""; // Sem max-age
    // Request/Response at T-10s
    entry.metadata.requestTime = now - seconds(10);
    entry.metadata.responseTime = now - seconds(10);
    // Date at T-10s
    entry.metadata.dateValue = now - seconds(10);
    // Last-Modified at T-110s (100s antes)
    entry.metadata.lastModifiedValue = now - seconds(110);
    
    // Heuristic = (Date - LastModified) * 0.1 = 100 * 0.1 = 10s
    // Current Age = 10s
    // 10 < 10 -> False (Stale ou limite)
    // Se passarmos 9s de resident time...
    
    // Vamos ajustar para ser Fresh
    // Date - LM = 200s -> Heuristic = 20s
    entry.metadata.lastModifiedValue = now - seconds(210);
    
    // Current Age = 10s
    // 10 < 20 -> Fresh
    
    HttpRequest req("http://dummy", HttpMethod::GET);
    // Bypass URL match for simplistic test
    entry.url = "http://dummy";
    
    auto status = CachePolicy::checkStatus(entry, req, now);
    assert(status == CacheResponseStatus::Fresh);
    
    // Se mudarmos para Stale
    // Age = 10s. Heuristic = 5s (Date - LM = 50s)
    entry.metadata.lastModifiedValue = now - seconds(60);
    status = CachePolicy::checkStatus(entry, req, now);
    assert(status == CacheResponseStatus::Stale);
    
    std::cout << "test_heuristic_freshness PASSED" << std::endl;
}

void test_stale_extension() {
    using namespace std::chrono;
    auto now = Clock::now();
    CacheEntry entry;
    entry.url = "http://example.com";
    entry.method = HttpMethod::GET;
    // Age = 10s
    entry.metadata.requestTime = now - seconds(10);
    entry.metadata.responseTime = now - seconds(10);
    entry.metadata.dateValue = now - seconds(10);
    
    // max-age=5 -> Freshness=5. Stale by 5s.
    // stale-while-revalidate=10 -> Window up to 15s total age.
    entry.metadata.cacheControl = "max-age=5, stale-while-revalidate=10";
    
    HttpRequest req("http://example.com", HttpMethod::GET);
    
    // 10s age < 5s fresh + 10s swr? Yes. -> StaleWhileRevalidate
    auto status = CachePolicy::checkStatus(entry, req, now);
    assert(status == CacheResponseStatus::StaleWhileRevalidate);
    
    // Age = 20s. 20 < 15? No. -> Stale
    entry.metadata.dateValue = now - seconds(20);
    entry.metadata.responseTime = now - seconds(20);
    entry.metadata.requestTime = now - seconds(20);
    
    status = CachePolicy::checkStatus(entry, req, now);
    assert(status == CacheResponseStatus::Stale);
    
    std::cout << "test_stale_extension PASSED" << std::endl;
}

int main() {
    test_is_cacheable();
    test_compute_age();
    test_freshness_check();
    test_heuristic_freshness();
    test_stale_extension();
    
    std::cout << "\n=== Todos os testes de Core Cache PASSARAM ===" << std::endl;
    return 0;
}
